/**
 * grid.js — Gridworld environment for interactive RL blog series.
 *
 * All sequential decision-making methods are approximations of expectimax;
 * this module provides the shared environment they all operate on.
 *
 * Exports: Actions, ActionNames, Grid, generateLayout, selfTest
 */

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

export const Actions = Object.freeze({ UP: 0, DOWN: 1, LEFT: 2, RIGHT: 3 });
export const ActionNames = ['UP', 'DOWN', 'LEFT', 'RIGHT'];

// Delta (dx, dy) for each action.  y increases downward (row index).
const DELTAS = [
    [0, -1],  // UP
    [0,  1],  // DOWN
    [-1, 0],  // LEFT
    [1,  0],  // RIGHT
];

// ---------------------------------------------------------------------------
// Reward constants
// ---------------------------------------------------------------------------

const REWARD_STEP   = -0.01;
const REWARD_ITEM   =  1.00;
const REWARD_EXIT   =  5.00;

// ---------------------------------------------------------------------------
// mulberry32 — deterministic 32-bit PRNG (seeded)
// ---------------------------------------------------------------------------

function mulberry32(seed) {
    let s = seed >>> 0;
    return function () {
        s += 0x6D2B79F5;
        let t = Math.imul(s ^ (s >>> 15), 1 | s);
        t = t + Math.imul(t ^ (t >>> 7), 61 | t) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

// ---------------------------------------------------------------------------
// Grid class
// ---------------------------------------------------------------------------

export class Grid {
    /**
     * @param {object} layout  Output of generateLayout (or hand-crafted object):
     *   { width, height, walls: Set<string>, items: Array<{x,y}>, exit: {x,y}, start: {x,y} }
     */
    constructor(layout) {
        this.width  = layout.width;
        this.height = layout.height;

        // Walls stored as "x,y" strings for O(1) lookup.
        this.walls = layout.walls instanceof Set
            ? new Set(layout.walls)
            : new Set(layout.walls);

        // Items: [{x, y}, …].  Index i corresponds to bit i in bitmask.
        this._items = layout.items.map(p => ({ x: p.x, y: p.y }));
        this.exit   = { x: layout.exit.x,  y: layout.exit.y  };
        this._start = { x: layout.start.x, y: layout.start.y };

        // Full bitmask when every item is present.
        this._fullMask = (1 << this._items.length) - 1;

        // Build item position → bit-index map for O(1) collection.
        this._itemIndex = new Map();
        for (let i = 0; i < this._items.length; i++) {
            const { x, y } = this._items[i];
            this._itemIndex.set(`${x},${y}`, i);
        }

        this.reset();
    }

    // -----------------------------------------------------------------------
    // Public getters for internal state fields
    // -----------------------------------------------------------------------

    /** Agent's current X position. */
    get x() { return this._x; }

    /** Agent's current Y position. */
    get y() { return this._y; }

    /** Current item-collection bitmask. */
    get bitmask() { return this._bitmask; }

    /**
     * Temporarily place the agent at (x, y), call fn(), then restore position.
     * Useful for value-map probing without exposing private fields.
     * @param {number} x
     * @param {number} y
     * @param {function} fn
     * @returns {*} Return value of fn()
     */
    withPosition(x, y, fn) {
        const origX = this._x, origY = this._y;
        this._x = x; this._y = y;
        try { return fn(); }
        finally { this._x = origX; this._y = origY; }
    }

    // -----------------------------------------------------------------------
    // State accessors
    // -----------------------------------------------------------------------

    /** Returns a plain object snapshot of the current state. */
    getState() {
        return { x: this._x, y: this._y, bitmask: this._bitmask };
    }

    /** String key suitable for Q-table lookup. */
    stateKey() {
        return `${this._x},${this._y},${this._bitmask}`;
    }

    /**
     * Total number of distinct (x, y, bitmask) states.
     * All cells × all bitmask values (even unreachable combos are counted for
     * simplicity — algorithms only visit reachable ones).
     */
    stateCount() {
        return this.width * this.height * (this._fullMask + 1);
    }

    // -----------------------------------------------------------------------
    // Environment interface
    // -----------------------------------------------------------------------

    /** Restore to initial state; return initial observation. */
    reset() {
        this._x       = this._start.x;
        this._y       = this._start.y;
        this._bitmask = this._fullMask;  // all items present
        this._done    = false;
        return this.getState();
    }

    /**
     * Take one step.
     * @param {number} action  One of Actions.{UP,DOWN,LEFT,RIGHT}
     * @returns {{ state: object, reward: number, done: boolean }}
     */
    step(action) {
        if (this._done) {
            throw new Error('step() called on a finished episode; call reset() first');
        }

        const [dx, dy] = DELTAS[action];
        const nx = this._x + dx;
        const ny = this._y + dy;

        // Wall or boundary check — agent stays put.
        if (this._blocked(nx, ny)) {
            // Even on a blocked move, check if standing on exit with all items.
            if (this._bitmask === 0
                    && this._x === this.exit.x && this._y === this.exit.y) {
                const reward = REWARD_STEP + REWARD_EXIT;
                this._done = true;
                return { state: this.getState(), reward, done: true };
            }
            return { state: this.getState(), reward: REWARD_STEP, done: false };
        }

        // Move agent.
        this._x = nx;
        this._y = ny;

        let reward = REWARD_STEP;
        let done   = false;

        // Item collection.
        const itemKey = `${nx},${ny}`;
        if (this._itemIndex.has(itemKey)) {
            const bit = this._itemIndex.get(itemKey);
            if (this._bitmask & (1 << bit)) {
                // Item is still present — collect it.
                this._bitmask &= ~(1 << bit);
                reward += REWARD_ITEM;
            }
        }

        // Exit check.
        if (nx === this.exit.x && ny === this.exit.y) {
            if (this._bitmask === 0) {
                // All items collected — episode complete.
                reward += REWARD_EXIT;
                done    = true;
                this._done = true;
            }
            // else: agent is on exit but episode continues
        }

        return { state: this.getState(), reward, done };
    }

    /** All four actions are always "legal"; walls simply block movement. */
    getLegalActions() {
        return [Actions.UP, Actions.DOWN, Actions.LEFT, Actions.RIGHT];
    }

    // -----------------------------------------------------------------------
    // Cell queries
    // -----------------------------------------------------------------------

    isWall(x, y) {
        return this.walls.has(`${x},${y}`);
    }

    isItem(x, y) {
        return this._itemIndex.has(`${x},${y}`);
    }

    /** Returns true only if the item at (x, y) has not yet been collected. */
    isItemPresent(x, y) {
        const key = `${x},${y}`;
        if (!this._itemIndex.has(key)) return false;
        const bit = this._itemIndex.get(key);
        return (this._bitmask & (1 << bit)) !== 0;
    }

    /** Read-only access to item positions (indices correspond to bitmask bits). */
    get items() {
        return this._items;
    }

    isExit(x, y) {
        return x === this.exit.x && y === this.exit.y;
    }

    // -----------------------------------------------------------------------
    // Clone
    // -----------------------------------------------------------------------

    /** Returns a fully independent copy of this environment. */
    clone() {
        const copy = new Grid({
            width:  this.width,
            height: this.height,
            walls:  this.walls,       // constructor copies the Set
            items:  this._items,
            exit:   this.exit,
            start:  this._start,
        });
        // Mirror current transient state.
        copy._x       = this._x;
        copy._y       = this._y;
        copy._bitmask = this._bitmask;
        copy._done    = this._done;
        return copy;
    }

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    _blocked(x, y) {
        return x < 0 || x >= this.width || y < 0 || y >= this.height
            || this.walls.has(`${x},${y}`);
    }
}

// ---------------------------------------------------------------------------
// _ensureReachable — BFS reachability helper for generateLayout
// ---------------------------------------------------------------------------

/**
 * For each target in `targets`, check if it is reachable from `start` through
 * non-wall cells.  For any unreachable target, find the shortest path from
 * `start` to `target` ignoring walls (pure BFS on all cells), then remove
 * every interior wall along that path.
 *
 * Mutates `walls` in place.
 *
 * @param {Set<string>} walls
 * @param {number} width
 * @param {number} height
 * @param {{x:number,y:number}} start
 * @param {Array<{x:number,y:number}>} targets
 */
function _ensureReachable(walls, width, height, start, targets) {
    const DIRS = [[0,-1],[0,1],[-1,0],[1,0]];

    /** BFS that respects current walls; returns Map of key→parent-key (null for start). */
    function bfsRespectingWalls(src) {
        const visited = new Map();
        visited.set(`${src.x},${src.y}`, null);
        const queue = [src];
        let head = 0;
        while (head < queue.length) {
            const { x, y } = queue[head++];
            for (const [dx, dy] of DIRS) {
                const nx = x + dx, ny = y + dy;
                const key = `${nx},${ny}`;
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                if (walls.has(key)) continue;
                if (visited.has(key)) continue;
                visited.set(key, `${x},${y}`);
                queue.push({ x: nx, y: ny });
            }
        }
        return visited;
    }

    /** BFS ignoring walls (all cells passable); returns Map of key→parent-key. */
    function bfsIgnoringWalls(src, dst) {
        const visited = new Map();
        visited.set(`${src.x},${src.y}`, null);
        const queue = [src];
        let head = 0;
        while (head < queue.length) {
            const { x, y } = queue[head++];
            const key = `${x},${y}`;
            if (x === dst.x && y === dst.y) return visited;
            for (const [dx, dy] of DIRS) {
                const nx = x + dx, ny = y + dy;
                const nkey = `${nx},${ny}`;
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                if (visited.has(nkey)) continue;
                visited.set(nkey, key);
                queue.push({ x: nx, y: ny });
            }
        }
        return visited; // dst unreachable (shouldn't happen on a finite grid)
    }

    /** Reconstruct path from BFS parent map as array of key strings. */
    function tracePath(parents, dstKey) {
        const path = [];
        let cur = dstKey;
        while (cur !== null) {
            path.push(cur);
            cur = parents.get(cur);
        }
        return path;
    }

    for (const target of targets) {
        const reachable = bfsRespectingWalls(start);
        const targetKey = `${target.x},${target.y}`;
        if (reachable.has(targetKey)) continue; // already reachable

        // Target is unreachable — carve a path from start to target.
        const parents = bfsIgnoringWalls(start, target);
        const path = tracePath(parents, targetKey);
        // Remove any interior wall along the path (never remove border walls).
        for (const key of path) {
            if (!walls.has(key)) continue;
            const [kx, ky] = key.split(',').map(Number);
            // Only remove interior walls (leave border intact).
            if (kx > 0 && kx < width - 1 && ky > 0 && ky < height - 1) {
                walls.delete(key);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// generateLayout
// ---------------------------------------------------------------------------

/**
 * Produce a deterministic grid layout from a seed.
 *
 * @param {object} opts
 * @param {number} opts.width       Grid width  (default 8)
 * @param {number} opts.height      Grid height (default 8)
 * @param {number} opts.nItems      Number of collectible items (default 3)
 * @param {string} opts.wallDensity 'none' | 'sparse' | 'maze' (default 'sparse')
 * @param {number} opts.seed        Integer seed for PRNG (default 42)
 * @returns {{ width, height, walls: Set<string>, items: Array<{x,y}>, exit: {x,y}, start: {x,y} }}
 */
export function generateLayout({
    width       = 8,
    height      = 8,
    nItems      = 3,
    wallDensity = 'sparse',
    seed        = 42,
} = {}) {
    const rand = mulberry32(seed);

    const DENSITY = { none: 0, sparse: 0.10, maze: 0.25 };
    const density = DENSITY[wallDensity] ?? 0.10;

    const walls = new Set();

    // Border walls.
    for (let x = 0; x < width; x++) {
        walls.add(`${x},0`);
        walls.add(`${x},${height - 1}`);
    }
    for (let y = 0; y < height; y++) {
        walls.add(`0,${y}`);
        walls.add(`${width - 1},${y}`);
    }

    // Fixed positions.
    const start = { x: 1, y: 1 };
    const exit  = { x: width - 2, y: height - 2 };

    // Interior walls (random, excluding start/exit).
    for (let y = 1; y < height - 1; y++) {
        for (let x = 1; x < width - 1; x++) {
            if (x === start.x && y === start.y) continue;
            if (x === exit.x  && y === exit.y)  continue;
            if (rand() < density) {
                walls.add(`${x},${y}`);
            }
        }
    }

    // Collect free interior cells (not walls, not start, not exit).
    const free = [];
    for (let y = 1; y < height - 1; y++) {
        for (let x = 1; x < width - 1; x++) {
            if (walls.has(`${x},${y}`))            continue;
            if (x === start.x && y === start.y)    continue;
            if (x === exit.x  && y === exit.y)     continue;
            free.push({ x, y });
        }
    }

    // Fisher-Yates partial shuffle to pick nItems positions.
    const count = Math.min(nItems, free.length);
    for (let i = 0; i < count; i++) {
        const j = i + Math.floor(rand() * (free.length - i));
        [free[i], free[j]] = [free[j], free[i]];
    }
    const items = free.slice(0, count);

    // ------------------------------------------------------------------
    // Reachability check: BFS from start; ensure exit and all items are
    // reachable.  For any unreachable target, trace back the BFS parent
    // map to find the shortest path from start and remove any walls on
    // that path.  This fixes ~32% unsolvable 'maze' layouts.
    // ------------------------------------------------------------------
    const targets = [exit, ...items];
    _ensureReachable(walls, width, height, start, targets);

    return { width, height, walls, items, exit, start };
}

// ---------------------------------------------------------------------------
// selfTest
// ---------------------------------------------------------------------------

/**
 * Run synchronous self-tests.  Returns an array of { name, passed, message } objects.
 * Designed to be called from test.html.
 */
export function selfTest() {
    const results = [];

    function test(name, fn) {
        try {
            fn();
            results.push({ name, passed: true, message: 'ok' });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    }

    function assert(cond, msg) {
        if (!cond) throw new Error(msg ?? 'assertion failed');
    }

    function assertClose(a, b, msg, tol = 1e-9) {
        if (Math.abs(a - b) > tol) throw new Error(msg ?? `${a} !== ${b}`);
    }

    // -------------------------------------------------------------------
    // Minimal 5×5 layout for deterministic testing.
    // Layout (x→ right, y↓ down):
    //   Walls on all borders (x=0,4 and y=0,4).
    //   start (1,1), exit (3,3), items at (2,1) and (1,2).
    // -------------------------------------------------------------------
    function makeSimpleLayout(extraWalls = []) {
        const walls = new Set();
        const W = 5, H = 5;
        for (let x = 0; x < W; x++) { walls.add(`${x},0`); walls.add(`${x},${H-1}`); }
        for (let y = 0; y < H; y++) { walls.add(`0,${y}`); walls.add(`${W-1},${y}`); }
        extraWalls.forEach(([x,y]) => walls.add(`${x},${y}`));
        return {
            width:  W,
            height: H,
            walls,
            items:  [{ x: 2, y: 1 }, { x: 1, y: 2 }],
            exit:   { x: 3, y: 3 },
            start:  { x: 1, y: 1 },
        };
    }

    // 1. Basic movement
    test('movement: move right', () => {
        const g = new Grid(makeSimpleLayout());
        const { state, reward, done } = g.step(Actions.RIGHT);
        assert(state.x === 2 && state.y === 1, `expected (2,1) got (${state.x},${state.y})`);
        // Moving onto item at (2,1): reward = step + item = -0.01 + 1 = 0.99
        assertClose(reward, 0.99, `reward should be 0.99 got ${reward}`);
        assert(!done, 'should not be done');
    });

    // 2. Wall collision — agent stays
    test('wall collision: blocked by border wall', () => {
        const g = new Grid(makeSimpleLayout());
        const s0 = g.getState();
        // UP from (1,1) hits border wall at y=0
        const { state, reward, done } = g.step(Actions.UP);
        assert(state.x === s0.x && state.y === s0.y,
            `agent should stay at (${s0.x},${s0.y}), got (${state.x},${state.y})`);
        assertClose(reward, -0.01, `wall bump reward should be -0.01 got ${reward}`);
        assert(!done, 'should not be done');
    });

    // 3. Interior wall collision
    test('wall collision: interior wall', () => {
        const g = new Grid(makeSimpleLayout([[2, 2]]));
        // Agent at (1,1). Move right → (2,1) (item), then DOWN → blocked by wall at (2,2)
        g.step(Actions.RIGHT); // collect item at (2,1), move to (2,1)
        const s1 = g.getState();
        const { state } = g.step(Actions.DOWN); // (2,2) is a wall
        assert(state.x === s1.x && state.y === s1.y,
            `agent should stay at (${s1.x},${s1.y}), got (${state.x},${state.y})`);
    });

    // 4. Item collection reward
    test('item collection: reward +0.99', () => {
        const g = new Grid(makeSimpleLayout());
        // Item at (2,1); move RIGHT from (1,1)
        const { reward } = g.step(Actions.RIGHT);
        assertClose(reward, 0.99, `expected 0.99 got ${reward}`);
        // Bitmask should have bit 0 cleared
        assert((g.getState().bitmask & 1) === 0, 'bit 0 should be cleared after collection');
    });

    // 5. Item not re-collected
    test('item not re-collected on revisit', () => {
        const g = new Grid(makeSimpleLayout());
        g.step(Actions.RIGHT);   // collect item at (2,1), now at (2,1)
        g.step(Actions.LEFT);    // move back to (1,1), no item there
        const { reward } = g.step(Actions.RIGHT); // revisit (2,1) — item gone
        assertClose(reward, -0.01, `re-visit should yield -0.01, got ${reward}`);
    });

    // 6. Exit without all items — not done
    test('exit without all items: not done', () => {
        // Simple 5×5 with items at (2,1) and (1,2).
        // Route to exit (3,3): RIGHT RIGHT DOWN DOWN
        const g = new Grid(makeSimpleLayout());
        g.step(Actions.RIGHT);  // (2,1) collect item bit0, reward 0.99
        g.step(Actions.RIGHT);  // (3,1)
        g.step(Actions.DOWN);   // (3,2)
        const { state, reward, done } = g.step(Actions.DOWN);  // (3,3) exit
        assert(state.x === 3 && state.y === 3, `should be at exit (3,3), got (${state.x},${state.y})`);
        assert(!done, 'should NOT be done — item at (1,2) still present');
        assertClose(reward, -0.01, `reward should be -0.01 got ${reward}`);
    });

    // 7. Exit with all items — done, reward +4.99
    test('exit with all items: done, reward +4.99', () => {
        const g = new Grid(makeSimpleLayout());
        // Collect item at (2,1): RIGHT
        g.step(Actions.RIGHT);            // (2,1) item collected
        // Collect item at (1,2): LEFT, DOWN
        g.step(Actions.LEFT);             // (1,1)
        g.step(Actions.DOWN);             // (1,2) item collected
        // Navigate to exit (3,3): RIGHT RIGHT DOWN
        g.step(Actions.RIGHT);            // (2,2)
        g.step(Actions.RIGHT);            // (3,2)
        const { state, reward, done } = g.step(Actions.DOWN);  // (3,3) exit
        assert(done, 'should be done');
        assertClose(reward, 4.99, `expected 4.99 got ${reward}`);
        assert(state.x === 3 && state.y === 3, `expected (3,3) got (${state.x},${state.y})`);
    });

    // 8. step() after done throws
    test('step after done throws', () => {
        const g = new Grid(makeSimpleLayout());
        g.step(Actions.RIGHT);
        g.step(Actions.LEFT);
        g.step(Actions.DOWN);
        g.step(Actions.RIGHT);
        g.step(Actions.RIGHT);
        g.step(Actions.DOWN);
        // Should be done now; next step should throw.
        let threw = false;
        try { g.step(Actions.UP); } catch (_) { threw = true; }
        assert(threw, 'step after done should throw');
    });

    // 9. reset() restores initial state
    test('reset restores initial state', () => {
        const g = new Grid(makeSimpleLayout());
        g.step(Actions.RIGHT);
        g.reset();
        const s = g.getState();
        assert(s.x === 1 && s.y === 1, `after reset, expected (1,1) got (${s.x},${s.y})`);
        assert(s.bitmask === 3, `after reset, bitmask should be 3, got ${s.bitmask}`);
    });

    // 10. stateKey format
    test('stateKey format: "x,y,bitmask"', () => {
        const g = new Grid(makeSimpleLayout());
        const key = g.stateKey();
        assert(key === '1,1,3', `expected "1,1,3" got "${key}"`);
        g.step(Actions.RIGHT); // collect item 0 at (2,1)
        const key2 = g.stateKey();
        assert(key2 === '2,1,2', `expected "2,1,2" got "${key2}"`);
    });

    // 11. stateCount
    test('stateCount = width * height * (fullMask+1)', () => {
        const g = new Grid(makeSimpleLayout());
        // 2 items → fullMask = 3, so 4 bitmask values; 5*5*4 = 100
        assert(g.stateCount() === 100, `expected 100 got ${g.stateCount()}`);
    });

    // 12. clone independence
    test('clone independence', () => {
        const g = new Grid(makeSimpleLayout());
        const c = g.clone();
        g.step(Actions.RIGHT);
        // c should still be at (1,1)
        assert(c.getState().x === 1, `clone should be unaffected, got x=${c.getState().x}`);
        // mutate clone; original unaffected
        c.step(Actions.DOWN);
        assert(g.getState().y === 1, `original should be unaffected by clone step`);
    });

    // 13. isWall / isItem / isExit
    test('isWall, isItem, isExit helpers', () => {
        const g = new Grid(makeSimpleLayout());
        assert(g.isWall(0, 0), 'corner (0,0) should be a wall');
        assert(!g.isWall(1, 1), '(1,1) is start, not a wall');
        assert(g.isItem(2, 1), '(2,1) should be an item');
        assert(!g.isItem(1, 1), '(1,1) should not be an item');
        assert(g.isExit(3, 3), '(3,3) should be the exit');
        assert(!g.isExit(1, 1), '(1,1) should not be the exit');
    });

    // 14. getLegalActions returns all four
    test('getLegalActions returns all 4 actions', () => {
        const g = new Grid(makeSimpleLayout());
        const actions = g.getLegalActions();
        assert(actions.length === 4, `expected 4 actions got ${actions.length}`);
        assert(actions.includes(Actions.UP), 'missing UP');
        assert(actions.includes(Actions.DOWN), 'missing DOWN');
        assert(actions.includes(Actions.LEFT), 'missing LEFT');
        assert(actions.includes(Actions.RIGHT), 'missing RIGHT');
    });

    // 15. generateLayout determinism
    test('generateLayout determinism: same seed → same layout', () => {
        const a = generateLayout({ width: 8, height: 8, nItems: 3, wallDensity: 'sparse', seed: 12345 });
        const b = generateLayout({ width: 8, height: 8, nItems: 3, wallDensity: 'sparse', seed: 12345 });
        // Same walls
        assert(a.walls.size === b.walls.size, 'wall counts differ');
        for (const w of a.walls) {
            assert(b.walls.has(w), `wall ${w} missing from second run`);
        }
        // Same items
        assert(a.items.length === b.items.length, 'item counts differ');
        for (let i = 0; i < a.items.length; i++) {
            assert(a.items[i].x === b.items[i].x && a.items[i].y === b.items[i].y,
                `item ${i} differs`);
        }
    });

    // 16. generateLayout: different seeds → different results (probabilistic)
    test('generateLayout: different seeds produce different layouts', () => {
        const a = generateLayout({ seed: 1 });
        const b = generateLayout({ seed: 9999 });
        // Extremely unlikely to be identical
        const sameWalls = a.walls.size === b.walls.size &&
            [...a.walls].every(w => b.walls.has(w));
        assert(!sameWalls, 'two different seeds should (almost certainly) differ');
    });

    // 17. generateLayout border walls always present
    test('generateLayout: border walls always present', () => {
        const { walls, width, height } = generateLayout({ wallDensity: 'none', seed: 1 });
        for (let x = 0; x < width; x++) {
            assert(walls.has(`${x},0`), `top border missing at ${x},0`);
            assert(walls.has(`${x},${height-1}`), `bottom border missing at ${x},${height-1}`);
        }
        for (let y = 0; y < height; y++) {
            assert(walls.has(`0,${y}`), `left border missing at 0,${y}`);
            assert(walls.has(`${width-1},${y}`), `right border missing at ${width-1},${y}`);
        }
    });

    // 18. generateLayout: start and exit not walls
    test('generateLayout: start and exit are not walls', () => {
        const { walls, start, exit } = generateLayout({ wallDensity: 'maze', seed: 7 });
        assert(!walls.has(`${start.x},${start.y}`), 'start should not be a wall');
        assert(!walls.has(`${exit.x},${exit.y}`),   'exit should not be a wall');
    });

    // 19. generateLayout: items not on walls, start, or exit
    test('generateLayout: items not on walls/start/exit', () => {
        const layout = generateLayout({ nItems: 5, wallDensity: 'sparse', seed: 42 });
        for (const item of layout.items) {
            assert(!layout.walls.has(`${item.x},${item.y}`),
                `item at (${item.x},${item.y}) is on a wall`);
            assert(!(item.x === layout.start.x && item.y === layout.start.y),
                `item at (${item.x},${item.y}) is on start`);
            assert(!(item.x === layout.exit.x && item.y === layout.exit.y),
                `item at (${item.x},${item.y}) is on exit`);
        }
    });

    // 20. isItemPresent reflects collection state
    test('isItemPresent: false after collection, true before', () => {
        const g = new Grid(makeSimpleLayout());
        assert(g.isItemPresent(2, 1), 'item at (2,1) should be present before collection');
        assert(g.isItemPresent(1, 2), 'item at (1,2) should be present before collection');
        assert(!g.isItemPresent(1, 1), '(1,1) has no item — should be false');
        g.step(Actions.RIGHT); // collect item at (2,1)
        assert(!g.isItemPresent(2, 1), 'item at (2,1) should not be present after collection');
        assert(g.isItemPresent(1, 2), 'item at (1,2) should still be present');
    });

    // 21. isItem is unaffected by collection state
    test('isItem: unchanged by collection state', () => {
        const g = new Grid(makeSimpleLayout());
        assert(g.isItem(2, 1), 'isItem(2,1) true before collection');
        g.step(Actions.RIGHT); // collect item at (2,1)
        assert(g.isItem(2, 1), 'isItem(2,1) still true after collection (layout query)');
    });

    // 22. items getter returns array of positions
    test('items getter returns item positions array', () => {
        const g = new Grid(makeSimpleLayout());
        const items = g.items;
        assert(Array.isArray(items), 'items should be an array');
        assert(items.length === 2, `expected 2 items got ${items.length}`);
        assert(items[0].x === 2 && items[0].y === 1, `items[0] should be (2,1)`);
        assert(items[1].x === 1 && items[1].y === 2, `items[1] should be (1,2)`);
    });

    // 23. exit check fires on wall-bump when agent is at exit with bitmask=0.
    // Directly inject state (position + bitmask) to isolate this path.
    test('exit check fires on wall-bump at exit with all items collected', () => {
        // Place agent at exit (3,3) with bitmask=0 (all items already collected).
        const g = new Grid(makeSimpleLayout());
        g._bitmask = 0;
        g._x = 3; g._y = 3;
        // DOWN from (3,3) hits the border wall at y=4 — normally returns early.
        // Fix: exit check must fire before the early return.
        const { reward, done } = g.step(Actions.DOWN);
        assert(done, 'should be done when bumping wall at exit with bitmask=0');
        // reward = REWARD_STEP + REWARD_EXIT = -0.01 + 5.00 = 4.99
        assertClose(reward, 4.99, `expected 4.99 got ${reward}`);
    });

    // 24. generateLayout reachability: exit and all items reachable from start (maze density)
    test('generateLayout reachability: exit and items reachable (maze density)', () => {
        const RDIRS = [[0,-1],[0,1],[-1,0],[1,0]];
        function bfsReachable(walls, width, height, src) {
            const visited = new Set();
            visited.add(`${src.x},${src.y}`);
            const queue = [src];
            let head = 0;
            while (head < queue.length) {
                const { x, y } = queue[head++];
                for (const [dx, dy] of RDIRS) {
                    const nx = x + dx, ny = y + dy;
                    const key = `${nx},${ny}`;
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    if (walls.has(key)) continue;
                    if (visited.has(key)) continue;
                    visited.add(key);
                    queue.push({ x: nx, y: ny });
                }
            }
            return visited;
        }
        // Test 50 seeds at the highest wall density.
        for (let seed = 0; seed < 50; seed++) {
            const layout = generateLayout({ wallDensity: 'maze', seed, nItems: 3 });
            const reachable = bfsReachable(layout.walls, layout.width, layout.height, layout.start);
            assert(reachable.has(`${layout.exit.x},${layout.exit.y}`),
                `seed ${seed}: exit unreachable`);
            for (const item of layout.items) {
                assert(reachable.has(`${item.x},${item.y}`),
                    `seed ${seed}: item (${item.x},${item.y}) unreachable`);
            }
        }
    });

    return results;
}
