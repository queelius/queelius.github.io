/**
 * tabular-q.js — Tabular Q-learning for the collection gridworld.
 *
 * Implements the standard Q-learning update:
 *   Q(s,a) += alpha * [r + gamma * max_a' Q(s',a') - Q(s,a)]
 *
 * Exports: TabularQ, selfTest
 */

// ---------------------------------------------------------------------------
// TabularQ
// ---------------------------------------------------------------------------

export class TabularQ {
    /**
     * @param {object} opts
     * @param {number} opts.alpha        Learning rate (default 0.1)
     * @param {number} opts.gamma        Discount factor (default 0.99)
     * @param {number} opts.epsilon      Initial exploration rate (default 1.0)
     * @param {number} opts.epsilonDecay Multiplicative decay per episode (default 0.995)
     * @param {number} opts.epsilonMin   Minimum exploration rate (default 0.01)
     */
    constructor({
        alpha        = 0.1,
        gamma        = 0.99,
        epsilon      = 1.0,
        epsilonDecay = 0.995,
        epsilonMin   = 0.01,
    } = {}) {
        this.alpha        = alpha;
        this.gamma        = gamma;
        this.epsilon      = epsilon;
        this.epsilonDecay = epsilonDecay;
        this.epsilonMin   = epsilonMin;

        // Q-table keyed by "stateKey|action"
        this._q       = new Map();
        this._episodes = 0;
    }

    // -----------------------------------------------------------------------
    // Public properties
    // -----------------------------------------------------------------------

    get name() { return 'Tabular Q'; }

    get episodes() { return this._episodes; }

    // -----------------------------------------------------------------------
    // Q-value access
    // -----------------------------------------------------------------------

    /** Get Q(s, a); returns 0 for unseen state-action pairs. */
    _getQ(stateKey, action) {
        const key = `${stateKey}|${action}`;
        return this._q.get(key) ?? 0;
    }

    /** Set Q(s, a). */
    _setQ(stateKey, action, value) {
        this._q.set(`${stateKey}|${action}`, value);
    }

    /** max_a Q(s, a) over the given legal actions. */
    _maxQ(stateKey, legalActions) {
        let best = -Infinity;
        for (const a of legalActions) {
            const q = this._getQ(stateKey, a);
            if (q > best) best = q;
        }
        return best === -Infinity ? 0 : best;
    }

    // -----------------------------------------------------------------------
    // Core RL interface
    // -----------------------------------------------------------------------

    /**
     * Epsilon-greedy action selection.
     *
     * @param {string|object} stateOrGrid  State key string, or a Grid object (calls stateKey())
     * @param {number[]}      legalActions Array of legal action indices
     * @returns {number} Selected action
     */
    selectAction(stateOrGrid, legalActions) {
        const stateKey = (typeof stateOrGrid === 'string')
            ? stateOrGrid
            : stateOrGrid.stateKey();
        if (Math.random() < this.epsilon) {
            // Explore: uniform random action
            return legalActions[Math.floor(Math.random() * legalActions.length)];
        }
        // Exploit: greedy action
        let bestAction = legalActions[0];
        let bestQ      = this._getQ(stateKey, bestAction);
        for (let i = 1; i < legalActions.length; i++) {
            const q = this._getQ(stateKey, legalActions[i]);
            if (q > bestQ) {
                bestQ      = q;
                bestAction = legalActions[i];
            }
        }
        return bestAction;
    }

    /**
     * Standard Q-learning update.
     *
     * @param {string}   stateKey      Current state key
     * @param {number}   action        Action taken
     * @param {number}   reward        Reward received
     * @param {string}   nextStateKey  Next state key
     * @param {number[]} nextLegalActions Legal actions from next state
     * @param {boolean}  done          Whether the episode ended
     */
    update(stateKey, action, reward, nextStateKey, nextLegalActions, done) {
        const currentQ = this._getQ(stateKey, action);
        const target   = done
            ? reward
            : reward + this.gamma * this._maxQ(nextStateKey, nextLegalActions);
        const newQ = currentQ + this.alpha * (target - currentQ);
        this._setQ(stateKey, action, newQ);
    }

    /**
     * End-of-episode bookkeeping: decay epsilon, increment episode counter.
     */
    endEpisode() {
        this.epsilon = Math.max(this.epsilonMin, this.epsilon * this.epsilonDecay);
        this._episodes++;
    }

    /**
     * Run one full training episode on the given grid.
     *
     * @param {Grid}   grid
     * @param {number} maxSteps Maximum steps per episode (default 200)
     * @returns {{ totalReward: number, steps: number }}
     */
    trainEpisode(grid, maxSteps = 200) {
        grid.reset();

        let totalReward = 0;
        let steps       = 0;
        let done        = false;

        let stateKey     = grid.stateKey();
        let legalActions = grid.getLegalActions();

        while (!done && steps < maxSteps) {
            const action = this.selectAction(stateKey, legalActions);
            const { reward, done: isDone } = grid.step(action);

            const nextStateKey     = grid.stateKey();
            const nextLegalActions = grid.getLegalActions();

            this.update(stateKey, action, reward, nextStateKey, nextLegalActions, isDone);

            totalReward  += reward;
            steps++;
            done          = isDone;
            stateKey      = nextStateKey;
            legalActions  = nextLegalActions;
        }

        this.endEpisode();
        return { totalReward, steps };
    }

    /**
     * Build a value map for the renderer.
     *
     * For each non-wall floor cell, computes max_a Q(s, a) at the grid's
     * current bitmask, then normalizes all values to [0, 1].
     *
     * @param {Grid} grid
     * @returns {Map<string, number>} stateKey → normalizedValue
     */
    getValueMap(grid) {
        const bitmask    = grid.bitmask;
        const legalActions = grid.getLegalActions();

        // Collect raw max-Q values for all non-wall cells.
        const raw = new Map();
        for (let y = 0; y < grid.height; y++) {
            for (let x = 0; x < grid.width; x++) {
                if (grid.isWall(x, y)) continue;
                const key = `${x},${y},${bitmask}`;
                const maxQ = this._maxQ(key, legalActions);
                if (maxQ !== 0) {
                    raw.set(key, maxQ);
                }
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
// selfTest
// ---------------------------------------------------------------------------

/**
 * Async self-test for TabularQ.
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

    function assert(cond, msg) {
        if (!cond) throw new Error(msg ?? 'assertion failed');
    }

    // -----------------------------------------------------------------------
    // Minimal test grid: 4x4 with border walls, 1 item at (2,1), exit (2,2)
    // start (1,1)
    //   #  #  #  #
    //   #  @  $  #
    //   #  .  >  #
    //   #  #  #  #
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
        const q = new TabularQ();
        assert(q.alpha        === 0.1,   `alpha: ${q.alpha}`);
        assert(q.gamma        === 0.99,  `gamma: ${q.gamma}`);
        assert(q.epsilon      === 1.0,   `epsilon: ${q.epsilon}`);
        assert(q.epsilonDecay === 0.995, `epsilonDecay: ${q.epsilonDecay}`);
        assert(q.epsilonMin   === 0.01,  `epsilonMin: ${q.epsilonMin}`);
        assert(q.episodes     === 0,     `episodes: ${q.episodes}`);
        assert(q.name         === 'Tabular Q', `name: ${q.name}`);
    });

    test('constructor: custom hyperparameters', () => {
        const q = new TabularQ({ alpha: 0.5, gamma: 0.9, epsilon: 0.5 });
        assert(q.alpha   === 0.5, `alpha: ${q.alpha}`);
        assert(q.gamma   === 0.9, `gamma: ${q.gamma}`);
        assert(q.epsilon === 0.5, `epsilon: ${q.epsilon}`);
    });

    test('_getQ returns 0 for unseen state-action', () => {
        const q = new TabularQ();
        assert(q._getQ('1,1,1', 0) === 0, 'should be 0');
    });

    test('update: Q-table is modified', () => {
        const q = new TabularQ({ alpha: 1.0 });
        q.update('s', 0, 1.0, 's2', [0, 1, 2, 3], false);
        // alpha=1: Q = 0 + 1*(1 + 0.99*0 - 0) = 1
        assert(q._getQ('s', 0) === 1.0, `expected 1.0 got ${q._getQ('s', 0)}`);
    });

    test('update: terminal state ignores next state value', () => {
        const q = new TabularQ({ alpha: 1.0, gamma: 0.99 });
        // Inject a high Q value for next state to ensure it is ignored when done=true
        q._setQ('s2', 0, 100);
        q.update('s', 0, 5.0, 's2', [0], true);
        // target = reward (done=true), Q = 0 + 1*(5.0 - 0) = 5
        assert(q._getQ('s', 0) === 5.0, `expected 5.0 got ${q._getQ('s', 0)}`);
    });

    test('endEpisode: epsilon decays and episodes increments', () => {
        const q = new TabularQ({ epsilon: 1.0, epsilonDecay: 0.5, epsilonMin: 0.1 });
        q.endEpisode();
        assert(q.episodes === 1, `episodes: ${q.episodes}`);
        assert(Math.abs(q.epsilon - 0.5) < 1e-9, `epsilon: ${q.epsilon}`);
        q.endEpisode();
        assert(q.episodes === 2, `episodes: ${q.episodes}`);
        assert(Math.abs(q.epsilon - 0.25) < 1e-9, `epsilon: ${q.epsilon}`);
    });

    test('endEpisode: epsilon does not go below epsilonMin', () => {
        const q = new TabularQ({ epsilon: 0.011, epsilonDecay: 0.5, epsilonMin: 0.01 });
        q.endEpisode();
        assert(q.epsilon === 0.01, `epsilon should be 0.01, got ${q.epsilon}`);
        q.endEpisode();
        assert(q.epsilon === 0.01, `epsilon should remain 0.01, got ${q.epsilon}`);
    });

    test('selectAction: explores when epsilon=1', () => {
        const q = new TabularQ({ epsilon: 1.0 });
        // With epsilon=1, always random — just verify it returns a valid action.
        const actions = [0, 1, 2, 3];
        for (let i = 0; i < 20; i++) {
            const a = q.selectAction('s', actions);
            assert(actions.includes(a), `invalid action: ${a}`);
        }
    });

    test('selectAction: exploits when epsilon=0', () => {
        const q = new TabularQ({ epsilon: 0.0 });
        q._setQ('s', 0, -1);
        q._setQ('s', 1,  5);
        q._setQ('s', 2,  2);
        q._setQ('s', 3,  0);
        const a = q.selectAction('s', [0, 1, 2, 3]);
        assert(a === 1, `expected action 1 (best Q=5), got ${a}`);
    });

    test('trainEpisode: returns totalReward and steps', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        const { totalReward, steps } = q.trainEpisode(grid);
        assert(typeof totalReward === 'number', 'totalReward should be a number');
        assert(typeof steps       === 'number', 'steps should be a number');
        assert(steps > 0,     'steps should be > 0');
        assert(steps <= 200,  'steps should be <= maxSteps');
    });

    test('trainEpisode: increments episode counter', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        assert(q.episodes === 0, 'before: 0 episodes');
        q.trainEpisode(grid);
        assert(q.episodes === 1, 'after 1 episode');
        q.trainEpisode(grid);
        assert(q.episodes === 2, 'after 2 episodes');
    });

    test('trainEpisode: Q-table populated after training', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        q.trainEpisode(grid);
        assert(q._q.size > 0, 'Q-table should be non-empty after training');
    });

    test('getValueMap: returns Map', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        for (let i = 0; i < 10; i++) q.trainEpisode(grid);
        const vm = q.getValueMap(grid);
        assert(vm instanceof Map, 'getValueMap should return a Map');
    });

    test('getValueMap: values in [0,1]', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        for (let i = 0; i < 50; i++) q.trainEpisode(grid);
        const vm = q.getValueMap(grid);
        for (const [key, val] of vm) {
            assert(val >= 0 && val <= 1, `value ${val} for key ${key} out of [0,1]`);
        }
    });

    test('getValueMap: empty before training', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new TabularQ();
        const vm   = q.getValueMap(grid);
        assert(vm instanceof Map,  'should return a Map');
        assert(vm.size === 0,      'should be empty before training');
    });

    // -----------------------------------------------------------------------
    // Integration test: 500 episodes on the tiny 4x4 grid
    // Assert average reward in last 50 episodes > 3
    // -----------------------------------------------------------------------
    await (async () => {
        const name = 'integration: 500 episodes, avg reward last 50 > 3';
        try {
            const grid = new Grid(makeTestLayout());
            // Use a faster-converging setup: higher alpha, faster epsilon decay
            const q    = new TabularQ({
                alpha:        0.3,
                gamma:        0.99,
                epsilon:      1.0,
                epsilonDecay: 0.99,
                epsilonMin:   0.01,
            });

            const rewardHistory = [];
            for (let i = 0; i < 500; i++) {
                const { totalReward } = q.trainEpisode(grid);
                rewardHistory.push(totalReward);
            }

            const last50  = rewardHistory.slice(-50);
            const avgLast = last50.reduce((s, v) => s + v, 0) / last50.length;

            assert(avgLast > 3,
                `avg reward in last 50 episodes should be > 3, got ${avgLast.toFixed(3)}`);
            assert(q._q.size > 0, 'Q-table should be non-empty');

            results.push({ name, passed: true, message: `avg=${avgLast.toFixed(3)}, Q-entries=${q._q.size}` });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    })();

    return results;
}
