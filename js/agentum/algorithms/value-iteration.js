/**
 * value-iteration.js — Full Bellman backup via state enumeration.
 *
 * Only feasible for small grids (≤10x10). The environment is deterministic,
 * so each (state, action) pair maps to exactly one next state.
 *
 * Exports: ValueIteration, selfTest
 */

// ---------------------------------------------------------------------------
// ValueIteration
// ---------------------------------------------------------------------------

export class ValueIteration {
    /**
     * @param {object} opts
     * @param {number} opts.gamma         Discount factor (default 0.99)
     * @param {number} opts.theta         Convergence threshold for max delta (default 1e-4)
     * @param {number} opts.maxIterations Maximum Bellman sweep iterations (default 200)
     */
    constructor({
        gamma         = 0.99,
        theta         = 1e-4,
        maxIterations = 200,
    } = {}) {
        this.gamma         = gamma;
        this.theta         = theta;
        this.maxIterations = maxIterations;

        // V(s): stateKey → value
        this._V          = new Map();
        // policy: stateKey → best action (number)
        this._policy     = new Map();
        // Transition model: stateKey → Map<action, { nextKey, reward, done }>
        this._transitions = new Map();
        // Set of terminal state keys
        this._terminals  = new Set();

        this._iterations = 0;
        this._converged  = false;
    }

    // -----------------------------------------------------------------------
    // Public properties
    // -----------------------------------------------------------------------

    get name() { return 'Value Iteration'; }

    /** Number of Bellman-sweep iterations performed by the last solve() call. */
    get iterations() { return this._iterations; }

    /** True if the last solve() converged within theta before maxIterations. */
    get converged() { return this._converged; }

    // -----------------------------------------------------------------------
    // solve
    // -----------------------------------------------------------------------

    /**
     * Compute V* for all reachable states via full Bellman backups.
     *
     * @param {Grid} grid  The environment (will be reset to start).
     */
    solve(grid) {
        // ------------------------------------------------------------------
        // 1. Enumerate all reachable states via BFS from the start state.
        //    We track (stateKey → cloned grid at that state) so we can
        //    compute transitions from each state.
        // ------------------------------------------------------------------
        const visited    = new Map(); // stateKey → grid clone at that state
        const queue      = [];

        grid.reset();
        const startKey = grid.stateKey();
        visited.set(startKey, grid.clone());
        queue.push(startKey);

        let head = 0;
        while (head < queue.length) {
            const key  = queue[head++];
            const node = visited.get(key);

            // Skip terminal states — no outgoing transitions.
            if (this._terminals.has(key)) continue;

            // Try all 4 actions from this state.
            for (const action of node.getLegalActions()) {
                const child = node.clone();
                const { reward, done } = child.step(action);
                const nextKey = child.stateKey();

                // Record transition for planning.
                if (!this._transitions.has(key)) {
                    this._transitions.set(key, new Map());
                }
                this._transitions.get(key).set(action, { nextKey, reward, done });

                if (done) {
                    this._terminals.add(nextKey);
                }

                if (!visited.has(nextKey)) {
                    visited.set(nextKey, child);
                    queue.push(nextKey);
                }
            }
        }

        // ------------------------------------------------------------------
        // 2. Initialize V(s) = 0 for all reachable states.
        // ------------------------------------------------------------------
        this._V.clear();
        this._policy.clear();
        for (const key of visited.keys()) {
            this._V.set(key, 0);
        }

        // ------------------------------------------------------------------
        // 3. Iterate Bellman backups until convergence.
        // ------------------------------------------------------------------
        this._iterations = 0;
        this._converged  = false;

        for (let iter = 0; iter < this.maxIterations; iter++) {
            let delta = 0;

            for (const [key, actionMap] of this._transitions) {
                if (this._terminals.has(key)) continue; // terminal: V = 0

                const vOld = this._V.get(key) ?? 0;
                let   bestValue  = -Infinity;
                let   bestAction = -1;

                for (const [action, { nextKey, reward, done }] of actionMap) {
                    const vNext = done ? 0 : (this._V.get(nextKey) ?? 0);
                    const value = reward + this.gamma * vNext;
                    if (value > bestValue) {
                        bestValue  = value;
                        bestAction = action;
                    }
                }

                if (bestValue === -Infinity) continue; // no actions (shouldn't happen)

                this._V.set(key, bestValue);
                this._policy.set(key, bestAction);

                const change = Math.abs(bestValue - vOld);
                if (change > delta) delta = change;
            }

            this._iterations++;

            if (delta < this.theta) {
                this._converged = true;
                break;
            }
        }

        // ------------------------------------------------------------------
        // 4. Extract policy for any states not yet set (e.g. states whose
        //    transitions were never updated because they converged at 0).
        // ------------------------------------------------------------------
        for (const [key, actionMap] of this._transitions) {
            if (this._policy.has(key)) continue;
            let bestValue  = -Infinity;
            let bestAction = -1;
            for (const [action, { nextKey, reward, done }] of actionMap) {
                const vNext = done ? 0 : (this._V.get(nextKey) ?? 0);
                const value = reward + this.gamma * vNext;
                if (value > bestValue) {
                    bestValue  = value;
                    bestAction = action;
                }
            }
            if (bestAction !== -1) this._policy.set(key, bestAction);
        }
    }

    // -----------------------------------------------------------------------
    // selectAction
    // -----------------------------------------------------------------------

    /**
     * Return the policy action for the grid's current state.
     * Falls back to action 0 (UP) if the state was not seen during solve().
     *
     * @param {Grid} grid
     * @returns {number} action
     */
    selectAction(grid) {
        const key    = grid.stateKey();
        const action = this._policy.get(key);
        return action !== undefined ? action : 0;
    }

    // -----------------------------------------------------------------------
    // getValueMap
    // -----------------------------------------------------------------------

    /**
     * Return a Map of stateKey → normalized value in [0, 1] for every state
     * that has a non-zero value in V*.
     *
     * Only states matching the grid's current bitmask are returned, making
     * it useful for the renderer (which shows one bitmask "layer" at a time).
     *
     * @param {Grid} grid
     * @returns {Map<string, number>}
     */
    getValueMap(grid) {
        const bitmask = grid.bitmask;

        // Collect raw V* values for states matching the current bitmask.
        const raw = new Map();
        for (const [key, val] of this._V) {
            // stateKey format: "x,y,bitmask"
            const lastComma = key.lastIndexOf(',');
            const keyBitmask = parseInt(key.slice(lastComma + 1), 10);
            if (keyBitmask === bitmask) {
                raw.set(key, val);
            }
        }

        if (raw.size === 0) return new Map();

        // Normalize to [0, 1] (loop avoids spread stack-overflow on large maps).
        let minVal = Infinity, maxVal = -Infinity;
        for (const v of raw.values()) {
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }
        const range = maxVal - minVal;

        const result = new Map();
        for (const [key, val] of raw) {
            result.set(key, range > 0 ? (val - minVal) / range : 1);
        }
        return result;
    }
}

// ---------------------------------------------------------------------------
// selfTest (async)
// ---------------------------------------------------------------------------

/**
 * Async self-test for ValueIteration.
 * Returns an array of { name, passed, message } objects.
 */
export async function selfTest() {
    const { Grid } = await import('../grid.js');

    const results = [];

    function test(name, fn) {
        try {
            fn();
            results.push({ name, passed: true, message: 'ok' });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    }

    async function testAsync(name, fn) {
        try {
            await fn();
            results.push({ name, passed: true, message: 'ok' });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    }

    function assert(cond, msg) {
        if (!cond) throw new Error(msg ?? 'assertion failed');
    }

    // -----------------------------------------------------------------------
    // 4x4 grid: border walls, 1 item at (2,1), exit at (2,2), start at (1,1).
    //
    //   #  #  #  #
    //   #  @  $  #
    //   #  .  >  #
    //   #  #  #  #
    //
    // Actions: UP=0, DOWN=1, LEFT=2, RIGHT=3
    // Optimal play from (1,1): RIGHT (collect item) → DOWN (reach exit)
    // -----------------------------------------------------------------------
    function makeTestLayout() {
        const W = 4, H = 4;
        const walls = new Set();
        for (let x = 0; x < W; x++) { walls.add(`${x},0`); walls.add(`${x},${H-1}`); }
        for (let y = 0; y < H; y++) { walls.add(`0,${y}`); walls.add(`${W-1},${y}`); }
        return {
            width:  W,
            height: H,
            walls,
            items:  [{ x: 2, y: 1 }],
            exit:   { x: 2, y: 2 },
            start:  { x: 1, y: 1 },
        };
    }

    // -----------------------------------------------------------------------
    // Unit tests
    // -----------------------------------------------------------------------

    test('constructor: default hyperparameters', () => {
        const vi = new ValueIteration();
        assert(vi.gamma         === 0.99,  `gamma: ${vi.gamma}`);
        assert(vi.theta         === 1e-4,  `theta: ${vi.theta}`);
        assert(vi.maxIterations === 200,   `maxIterations: ${vi.maxIterations}`);
        assert(vi.iterations    === 0,     `iterations initially 0`);
        assert(vi.converged     === false, `converged initially false`);
        assert(vi.name          === 'Value Iteration', `name: ${vi.name}`);
    });

    test('constructor: custom hyperparameters', () => {
        const vi = new ValueIteration({ gamma: 0.9, theta: 1e-6, maxIterations: 50 });
        assert(vi.gamma         === 0.9,   `gamma: ${vi.gamma}`);
        assert(vi.theta         === 1e-6,  `theta: ${vi.theta}`);
        assert(vi.maxIterations === 50,    `maxIterations: ${vi.maxIterations}`);
    });

    // -----------------------------------------------------------------------
    // Integration test: solve the 4x4 grid and check convergence + policy
    // -----------------------------------------------------------------------
    await testAsync('integration: solve converges on 4x4 grid', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration();

        vi.solve(grid);

        assert(vi.converged,
            `expected convergence within ${vi.maxIterations} iterations; got ${vi.iterations}`);
        assert(vi.iterations > 0,
            `iterations should be > 0, got ${vi.iterations}`);
    });

    await testAsync('integration: V(start) > 3', async () => {
        const grid    = new Grid(makeTestLayout());
        const vi      = new ValueIteration();
        vi.solve(grid);

        grid.reset();
        const startKey = grid.stateKey();
        const vStart   = vi._V.get(startKey);
        assert(vStart !== undefined, `V(start) not found for key ${startKey}`);
        assert(vStart > 3,
            `V(start) should be > 3 (item + exit - small step costs), got ${vStart.toFixed(4)}`);
    });

    await testAsync('integration: policy does not recommend LEFT from start', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration();
        vi.solve(grid);

        const action = vi.selectAction(grid);
        // LEFT (2) from (1,1) bumps into the left border wall — clearly suboptimal.
        assert(action !== 2,
            `policy recommended LEFT (2) from start — should not, got action ${action}`);
    });

    await testAsync('integration: optimal policy reaches exit with item collected', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration();
        vi.solve(grid);

        grid.reset();
        let done  = false;
        let steps = 0;
        const MAX = 20;

        while (!done && steps < MAX) {
            const action = vi.selectAction(grid);
            const result = grid.step(action);
            done  = result.done;
            steps++;
        }

        assert(done,
            `policy should reach a terminal state within ${MAX} steps (took ${steps})`);
    });

    await testAsync('getValueMap: returns Map with values in [0,1]', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration();
        vi.solve(grid);

        grid.reset();
        const vm = vi.getValueMap(grid);
        assert(vm instanceof Map, 'getValueMap should return a Map');
        assert(vm.size > 0,      'value map should be non-empty after solve');

        for (const [key, val] of vm) {
            assert(val >= 0 && val <= 1,
                `value ${val} for key ${key} out of [0,1]`);
        }
    });

    await testAsync('getValueMap: filters by current bitmask', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration();
        vi.solve(grid);

        // Reset to initial state (bitmask = 1, one item present).
        grid.reset();
        const bitmask = grid.bitmask;
        const vm      = vi.getValueMap(grid);

        for (const key of vm.keys()) {
            const lastComma    = key.lastIndexOf(',');
            const keyBitmask   = parseInt(key.slice(lastComma + 1), 10);
            assert(keyBitmask === bitmask,
                `key ${key} has bitmask ${keyBitmask}, expected ${bitmask}`);
        }
    });

    await testAsync('selectAction: falls back to 0 for unseen state', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration(); // never solved
        const action = vi.selectAction(grid);
        assert(action === 0, `fallback action should be 0, got ${action}`);
    });

    await testAsync('solve: respects maxIterations limit', async () => {
        const grid = new Grid(makeTestLayout());
        const vi   = new ValueIteration({ maxIterations: 1, theta: 0 }); // 1 iteration only
        vi.solve(grid);
        assert(vi.iterations === 1,
            `expected 1 iteration, got ${vi.iterations}`);
        assert(!vi.converged,
            'should not be marked converged after only 1 iteration with theta=0');
    });

    return results;
}
