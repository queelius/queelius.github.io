/**
 * linear-q.js — Linear Q-learning using hand-crafted features.
 *
 * Implements semi-gradient TD(0) Q-learning with linear function approximation:
 *   Q(s, a; w_a) = w_a · φ(s)
 *   w_a += α * [r + γ * max_a' Q(s', a') - Q(s, a)] * φ(s)
 *
 * Exports: LinearQ, selfTest
 */

import { extractFeatures, DEFAULT_FEATURE_KEYS } from '../features.js';

// Number of actions (UP, DOWN, LEFT, RIGHT)
const N_ACTIONS = 4;

// ---------------------------------------------------------------------------
// LinearQ
// ---------------------------------------------------------------------------

export class LinearQ {
    /**
     * @param {object}   opts
     * @param {string[]} opts.featureKeys  Feature keys to use (default: DEFAULT_FEATURE_KEYS)
     * @param {number}   opts.alpha        Learning rate (default 0.01)
     * @param {number}   opts.gamma        Discount factor (default 0.99)
     * @param {number}   opts.epsilon      Initial exploration rate (default 1.0)
     * @param {number}   opts.epsilonDecay Multiplicative decay per episode (default 0.995)
     * @param {number}   opts.epsilonMin   Minimum exploration rate (default 0.01)
     */
    constructor({
        featureKeys  = DEFAULT_FEATURE_KEYS,
        alpha        = 0.01,
        gamma        = 0.99,
        epsilon      = 1.0,
        epsilonDecay = 0.995,
        epsilonMin   = 0.01,
    } = {}) {
        this.featureKeys  = featureKeys;
        this.alpha        = alpha;
        this.gamma        = gamma;
        this.epsilon      = epsilon;
        this.epsilonDecay = epsilonDecay;
        this.epsilonMin   = epsilonMin;

        // Weight dimension: one weight per feature plus the bias term.
        this._dim = featureKeys.length + 1;

        // One weight vector per action, initialized to zero.
        this._weights = Array.from({ length: N_ACTIONS }, () =>
            new Float64Array(this._dim)
        );

        this._episodes = 0;
    }

    // -----------------------------------------------------------------------
    // Public properties
    // -----------------------------------------------------------------------

    get name() { return 'Linear Q'; }

    get episodes() { return this._episodes; }

    // -----------------------------------------------------------------------
    // Q-value computation
    // -----------------------------------------------------------------------

    /** Compute Q(s, a) = w_a · φ. */
    _qValue(features, action) {
        const w = this._weights[action];
        let q = 0;
        for (let i = 0; i < features.length; i++) q += w[i] * features[i];
        return q;
    }

    /** max_a Q(s, a) over the given legal actions given a feature vector. */
    _maxQ(features, legalActions) {
        let best = -Infinity;
        for (const a of legalActions) {
            const q = this._qValue(features, a);
            if (q > best) best = q;
        }
        return best === -Infinity ? 0 : best;
    }

    // -----------------------------------------------------------------------
    // Core RL interface
    // -----------------------------------------------------------------------

    /**
     * Epsilon-greedy action selection using a pre-extracted feature vector.
     * Used internally by trainEpisode to avoid double extraction.
     *
     * @param {number[]} features     Pre-extracted feature vector
     * @param {number[]} legalActions Legal action indices
     * @returns {number} Selected action
     */
    _selectActionFromFeatures(features, legalActions) {
        let bestAction = legalActions[0];
        let bestQ      = this._qValue(features, bestAction);
        for (let i = 1; i < legalActions.length; i++) {
            const q = this._qValue(features, legalActions[i]);
            if (q > bestQ) {
                bestQ      = q;
                bestAction = legalActions[i];
            }
        }
        return bestAction;
    }

    /**
     * Epsilon-greedy action selection.
     *
     * @param {Grid}     grid         The environment (read its state via features)
     * @param {number[]} legalActions Legal action indices
     * @returns {number} Selected action
     */
    selectAction(grid, legalActions) {
        if (Math.random() < this.epsilon) {
            return legalActions[Math.floor(Math.random() * legalActions.length)];
        }

        const features = extractFeatures(grid, this.featureKeys);
        return this._selectActionFromFeatures(features, legalActions);
    }

    /**
     * Semi-gradient TD update:
     *   w_a += α * (target - Q(s, a; w_a)) * φ(s)
     *
     * @param {number[]} features          Feature vector φ(s)
     * @param {number}   action            Action taken
     * @param {number}   reward            Reward received
     * @param {number[]} nextFeatures      Feature vector φ(s')
     * @param {number[]} nextLegalActions  Legal actions from s'
     * @param {boolean}  done              Whether the episode ended
     */
    update(features, action, reward, nextFeatures, nextLegalActions, done) {
        const currentQ = this._qValue(features, action);
        const target   = done
            ? reward
            : reward + this.gamma * this._maxQ(nextFeatures, nextLegalActions);

        const delta = this.alpha * (target - currentQ);
        const w = this._weights[action];
        for (let i = 0; i < features.length; i++) {
            w[i] += delta * features[i];
        }
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

        let totalReward  = 0;
        let steps        = 0;
        let done         = false;

        let features     = extractFeatures(grid, this.featureKeys);
        let legalActions = grid.getLegalActions();

        while (!done && steps < maxSteps) {
            const action = Math.random() < this.epsilon
                ? legalActions[Math.floor(Math.random() * legalActions.length)]
                : this._selectActionFromFeatures(features, legalActions);
            const { reward, done: isDone } = grid.step(action);

            const nextFeatures     = extractFeatures(grid, this.featureKeys);
            const nextLegalActions = grid.getLegalActions();

            this.update(features, action, reward, nextFeatures, nextLegalActions, isDone);

            totalReward  += reward;
            steps++;
            done          = isDone;
            features      = nextFeatures;
            legalActions  = nextLegalActions;
        }

        this.endEpisode();
        return { totalReward, steps };
    }

    /**
     * Build a value map for the renderer.
     *
     * For each non-wall cell, temporarily sets the agent position to that cell,
     * extracts features, and computes max_a Q(s, a).  Normalizes values to [0, 1].
     * Restores the original agent position afterward.
     *
     * @param {Grid} grid
     * @returns {Map<string, number>} "x,y,bitmask" → normalizedValue
     */
    getValueMap(grid) {
        const bitmask      = grid.bitmask;
        const legalActions = grid.getLegalActions();

        const raw = new Map();

        for (let y = 0; y < grid.height; y++) {
            for (let x = 0; x < grid.width; x++) {
                if (grid.isWall(x, y)) continue;

                const features = grid.withPosition(x, y, () => extractFeatures(grid, this.featureKeys));
                const maxQ     = this._maxQ(features, legalActions);
                raw.set(`${x},${y},${bitmask}`, maxQ);
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
 * Async self-test for LinearQ.
 * Returns an array of { name, passed, message } objects.
 */
export async function selfTest() {
    const { Grid } = await import('../grid.js');
    const { FEATURES, extractFeatures: ef, ALL_FEATURE_KEYS, DEFAULT_FEATURE_KEYS } =
        await import('../features.js');

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
    // Minimal 5×5 layout: border walls, 1 item, agent at (1,1), exit at (3,3)
    //   # # # # #
    //   # @ . . #
    //   # . . . #
    //   # . . > #
    //   # # # # #
    // -----------------------------------------------------------------------
    function makeTestLayout() {
        const W = 5, H = 5;
        const walls = new Set();
        for (let x = 0; x < W; x++) {
            walls.add(`${x},0`);
            walls.add(`${x},${H - 1}`);
        }
        for (let y = 0; y < H; y++) {
            walls.add(`0,${y}`);
            walls.add(`${W - 1},${y}`);
        }
        return {
            width:  W,
            height: H,
            walls,
            items:  [{ x: 2, y: 2 }],
            exit:   { x: 3, y: 3 },
            start:  { x: 1, y: 1 },
        };
    }

    // -----------------------------------------------------------------------
    // features.js unit tests
    // -----------------------------------------------------------------------

    test('features: FEATURES is a non-empty array', () => {
        assert(Array.isArray(FEATURES) && FEATURES.length > 0, 'FEATURES should be non-empty array');
    });

    test('features: ALL_FEATURE_KEYS has correct length', () => {
        assert(ALL_FEATURE_KEYS.length === FEATURES.length,
            `ALL_FEATURE_KEYS length ${ALL_FEATURE_KEYS.length} !== FEATURES length ${FEATURES.length}`);
    });

    test('features: DEFAULT_FEATURE_KEYS is subset of ALL_FEATURE_KEYS', () => {
        for (const k of DEFAULT_FEATURE_KEYS) {
            assert(ALL_FEATURE_KEYS.includes(k), `${k} not in ALL_FEATURE_KEYS`);
        }
    });

    test('features: each feature has required fields', () => {
        for (const f of FEATURES) {
            assert(typeof f.name        === 'string',   `${f.key}: name must be string`);
            assert(typeof f.key         === 'string',   `name "${f.name}": key must be string`);
            assert(typeof f.description === 'string',   `${f.key}: description must be string`);
            assert(typeof f.extract     === 'function', `${f.key}: extract must be function`);
        }
    });

    test('features: extract returns numbers in [0,1] for test grid', () => {
        const grid = new Grid(makeTestLayout());
        for (const f of FEATURES) {
            const val = f.extract(grid);
            assert(typeof val === 'number', `${f.key}: extract must return number, got ${typeof val}`);
            assert(val >= 0 && val <= 1,    `${f.key}: value ${val} out of [0,1]`);
        }
    });

    test('extractFeatures: length = activeKeys.length + 1 (bias)', () => {
        const grid = new Grid(makeTestLayout());
        const vec  = ef(grid, DEFAULT_FEATURE_KEYS);
        assert(vec.length === DEFAULT_FEATURE_KEYS.length + 1,
            `expected length ${DEFAULT_FEATURE_KEYS.length + 1}, got ${vec.length}`);
    });

    test('extractFeatures: last element is 1 (bias term)', () => {
        const grid = new Grid(makeTestLayout());
        const vec  = ef(grid, DEFAULT_FEATURE_KEYS);
        assert(vec[vec.length - 1] === 1.0,
            `bias term should be 1.0, got ${vec[vec.length - 1]}`);
    });

    test('extractFeatures: items_remaining = 1 with all items present', () => {
        const grid = new Grid(makeTestLayout());
        const vec  = ef(grid, ['items_remaining']);
        assert(vec[0] === 1.0, `items_remaining should be 1.0, got ${vec[0]}`);
    });

    test('extractFeatures: items_remaining = 0 after collecting all items', () => {
        const grid = new Grid(makeTestLayout());
        // Collect the single item at (2,2): RIGHT, DOWN
        grid.step(1); // DOWN to (1,2)
        grid.step(3); // RIGHT to (2,2) — collect item
        const vec = ef(grid, ['items_remaining']);
        assert(vec[0] === 0.0, `items_remaining should be 0.0 after collection, got ${vec[0]}`);
    });

    test('features: pos_x normalized correctly at start (1,1)', () => {
        const grid = new Grid(makeTestLayout()); // width=5, start.x=1
        const val  = FEATURES.find(f => f.key === 'pos_x').extract(grid);
        // x=1, width=5, maxX=4 → 1/4 = 0.25
        assert(Math.abs(val - 0.25) < 1e-9, `pos_x: expected 0.25, got ${val}`);
    });

    test('features: exit_dist = 0 when agent is on exit', () => {
        const grid = new Grid(makeTestLayout()); // exit at (3,3)
        const val  = grid.withPosition(3, 3, () => FEATURES.find(f => f.key === 'exit_dist').extract(grid));
        assert(val === 0, `exit_dist should be 0, got ${val}`);
    });

    // -----------------------------------------------------------------------
    // LinearQ unit tests
    // -----------------------------------------------------------------------

    test('LinearQ constructor: default hyperparameters', () => {
        const q = new LinearQ();
        assert(q.alpha        === 0.01,  `alpha: ${q.alpha}`);
        assert(q.gamma        === 0.99,  `gamma: ${q.gamma}`);
        assert(q.epsilon      === 1.0,   `epsilon: ${q.epsilon}`);
        assert(q.epsilonDecay === 0.995, `epsilonDecay: ${q.epsilonDecay}`);
        assert(q.epsilonMin   === 0.01,  `epsilonMin: ${q.epsilonMin}`);
        assert(q.episodes     === 0,     `episodes: ${q.episodes}`);
        assert(q.name         === 'Linear Q', `name: "${q.name}"`);
    });

    test('LinearQ constructor: custom featureKeys', () => {
        const keys = ['pos_x', 'pos_y'];
        const q    = new LinearQ({ featureKeys: keys });
        assert(q.featureKeys === keys, 'featureKeys should be stored');
        // dim = 2 features + 1 bias = 3
        assert(q._dim === 3, `_dim should be 3, got ${q._dim}`);
        assert(q._weights.length === N_ACTIONS, `_weights should have ${N_ACTIONS} vectors`);
        assert(q._weights[0] instanceof Float64Array, 'weight vectors should be Float64Array');
        assert(q._weights[0].length === 3, `weight vector length should be 3`);
    });

    test('LinearQ: initial weights are zero', () => {
        const q = new LinearQ();
        for (const w of q._weights) {
            for (const v of w) {
                assert(v === 0, `weight should be 0, got ${v}`);
            }
        }
    });

    test('LinearQ: _qValue is 0 with zero weights', () => {
        const q        = new LinearQ({ featureKeys: ['pos_x'] });
        const features = [0.5, 1.0]; // [pos_x, bias]
        assert(q._qValue(features, 0) === 0, 'Q with zero weights should be 0');
    });

    test('LinearQ: update modifies weights in correct direction', () => {
        const q = new LinearQ({ featureKeys: ['pos_x'], alpha: 0.1, gamma: 0.99 });
        const features     = [0.5, 1.0];
        const nextFeatures  = [0.5, 1.0];
        // reward=1, done=true → target=1, error=1-0=1, Δw = 0.1 * 1 * features
        q.update(features, 0, 1.0, nextFeatures, [0, 1, 2, 3], true);
        const w = q._weights[0];
        assert(Math.abs(w[0] - 0.05) < 1e-9, `w[0] should be 0.05, got ${w[0]}`);
        assert(Math.abs(w[1] - 0.10) < 1e-9, `w[1] should be 0.10, got ${w[1]}`);
        // Other action weights unchanged
        assert(q._weights[1][0] === 0, 'action 1 weight should be 0');
    });

    test('LinearQ: endEpisode decays epsilon and increments counter', () => {
        const q = new LinearQ({ epsilon: 1.0, epsilonDecay: 0.5, epsilonMin: 0.1 });
        q.endEpisode();
        assert(q.episodes === 1, `episodes: ${q.episodes}`);
        assert(Math.abs(q.epsilon - 0.5) < 1e-9, `epsilon: ${q.epsilon}`);
        q.endEpisode();
        assert(q.episodes === 2, `episodes: ${q.episodes}`);
        assert(Math.abs(q.epsilon - 0.25) < 1e-9, `epsilon: ${q.epsilon}`);
    });

    test('LinearQ: endEpisode: epsilon does not go below epsilonMin', () => {
        const q = new LinearQ({ epsilon: 0.011, epsilonDecay: 0.5, epsilonMin: 0.01 });
        q.endEpisode();
        assert(q.epsilon === 0.01, `epsilon should be 0.01, got ${q.epsilon}`);
    });

    test('LinearQ: selectAction returns valid action', () => {
        const grid   = new Grid(makeTestLayout());
        const q      = new LinearQ({ epsilon: 0.0 }); // exploit
        const legal  = grid.getLegalActions();
        for (let i = 0; i < 10; i++) {
            const a = q.selectAction(grid, legal);
            assert(legal.includes(a), `invalid action: ${a}`);
        }
    });

    test('LinearQ: trainEpisode returns totalReward and steps', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new LinearQ();
        const { totalReward, steps } = q.trainEpisode(grid);
        assert(typeof totalReward === 'number', 'totalReward should be a number');
        assert(typeof steps       === 'number', 'steps should be a number');
        assert(steps > 0,    'steps should be > 0');
        assert(steps <= 200, 'steps should be <= maxSteps');
    });

    test('LinearQ: trainEpisode increments episode counter', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new LinearQ();
        assert(q.episodes === 0, 'before: 0 episodes');
        q.trainEpisode(grid);
        assert(q.episodes === 1, 'after 1 episode');
        q.trainEpisode(grid);
        assert(q.episodes === 2, 'after 2 episodes');
    });

    test('LinearQ: getValueMap returns a Map', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new LinearQ();
        for (let i = 0; i < 10; i++) q.trainEpisode(grid);
        const vm = q.getValueMap(grid);
        assert(vm instanceof Map, 'getValueMap should return a Map');
    });

    test('LinearQ: getValueMap values in [0,1]', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new LinearQ();
        for (let i = 0; i < 20; i++) q.trainEpisode(grid);
        const vm = q.getValueMap(grid);
        for (const [key, val] of vm) {
            assert(val >= 0 && val <= 1, `value ${val} for key "${key}" out of [0,1]`);
        }
    });

    test('LinearQ: getValueMap restores agent position', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new LinearQ();
        const origX = grid.x;
        const origY = grid.y;
        q.getValueMap(grid);
        assert(grid.x === origX && grid.y === origY,
            `position not restored: expected (${origX},${origY}), got (${grid.x},${grid.y})`);
    });

    // -----------------------------------------------------------------------
    // Integration test: 500 episodes on the tiny 5×5 grid
    // Assert average reward in last 50 episodes > 2
    // Assert weights are non-zero after training
    // -----------------------------------------------------------------------
    await (async () => {
        const name = 'integration: 500 episodes, avg reward last 50 > 2, weights non-zero';
        try {
            const grid = new Grid(makeTestLayout());
            const q    = new LinearQ({
                featureKeys:  DEFAULT_FEATURE_KEYS,
                alpha:        0.05,
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

            assert(avgLast > 2,
                `avg reward in last 50 episodes should be > 2, got ${avgLast.toFixed(3)}`);

            // Check that at least some weights are non-zero
            let anyNonZero = false;
            for (const w of q._weights) {
                for (const v of w) {
                    if (v !== 0) { anyNonZero = true; break; }
                }
                if (anyNonZero) break;
            }
            assert(anyNonZero, 'weights should be non-zero after training');

            results.push({ name, passed: true,
                message: `avg=${avgLast.toFixed(3)}, epsilon=${q.epsilon.toFixed(4)}` });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    })();

    return results;
}
