/**
 * neural-q.js — Neural Q-learning for the collection gridworld.
 *
 * Hand-rolled 2-layer MLP Q-network with experience replay and a target
 * network.  No external libraries; all math is plain JS Float64Arrays.
 *
 * Architecture: input → Linear → ReLU → Linear → Q-values (one per action)
 *
 * Exports: NeuralQ, selfTest
 */

import { extractFeatures, DEFAULT_FEATURE_KEYS } from '../features.js';

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

/** Mulberry32 PRNG — seeded, deterministic (used only for weight init). */
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
// MLP — 2-layer multi-layer perceptron (internal, not exported)
// ---------------------------------------------------------------------------

class MLP {
    /**
     * @param {number} inputDim   Number of input features
     * @param {number} hiddenDim  Number of hidden units
     * @param {number} outputDim  Number of outputs (= number of actions)
     */
    constructor(inputDim, hiddenDim, outputDim) {
        this.inputDim  = inputDim;
        this.hiddenDim = hiddenDim;
        this.outputDim = outputDim;

        // Layer 1: inputDim → hiddenDim  (weights + biases)
        this.W1 = new Float64Array(hiddenDim * inputDim);
        this.b1 = new Float64Array(hiddenDim);

        // Layer 2: hiddenDim → outputDim
        this.W2 = new Float64Array(outputDim * hiddenDim);
        this.b2 = new Float64Array(outputDim);

        this._initXavier();
    }

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    /** Xavier (Glorot) uniform initialization. */
    _initXavier() {
        const rand = mulberry32(0xDEADBEEF);

        const limit1 = Math.sqrt(6 / (this.inputDim  + this.hiddenDim));
        for (let i = 0; i < this.W1.length; i++) {
            this.W1[i] = (rand() * 2 - 1) * limit1;
        }

        const limit2 = Math.sqrt(6 / (this.hiddenDim + this.outputDim));
        for (let i = 0; i < this.W2.length; i++) {
            this.W2[i] = (rand() * 2 - 1) * limit2;
        }
        // Biases initialize to zero (already done by Float64Array constructor).
    }

    // -----------------------------------------------------------------------
    // Forward pass
    // -----------------------------------------------------------------------

    /**
     * Run a forward pass.
     *
     * @param {Float64Array} input  Shape: [inputDim]
     * @returns {{ output: Float64Array, hidden: Float64Array, input: Float64Array }}
     *   Cache object required by backward().
     */
    forward(input) {
        const { inputDim, hiddenDim, outputDim, W1, b1, W2, b2 } = this;

        // Layer 1: z1 = W1 · input + b1, h = ReLU(z1)
        const hidden = new Float64Array(hiddenDim);
        for (let j = 0; j < hiddenDim; j++) {
            let sum = b1[j];
            const rowOffset = j * inputDim;
            for (let i = 0; i < inputDim; i++) {
                sum += W1[rowOffset + i] * input[i];
            }
            hidden[j] = sum > 0 ? sum : 0; // ReLU
        }

        // Layer 2: output = W2 · hidden + b2  (linear, no activation)
        const output = new Float64Array(outputDim);
        for (let k = 0; k < outputDim; k++) {
            let sum = b2[k];
            const rowOffset = k * hiddenDim;
            for (let j = 0; j < hiddenDim; j++) {
                sum += W2[rowOffset + j] * hidden[j];
            }
            output[k] = sum;
        }

        return { output, hidden, input };
    }

    // -----------------------------------------------------------------------
    // Backward pass — MSE loss, SGD update
    // -----------------------------------------------------------------------

    /**
     * Compute MSE gradients and apply an SGD update in-place.
     *
     * Only the output elements specified by `actionIndices` are trained;
     * the others are left at their current predicted values so they produce
     * zero gradient (standard DQN single-action target approach).
     *
     * @param {{ output: Float64Array, hidden: Float64Array, input: Float64Array }} cache
     *   Return value of the matching forward() call.
     * @param {Float64Array} targetOutput  Desired Q-values (full outputDim vector).
     * @param {number}       lr            Learning rate.
     * @param {number}       [gradClip=1.0] Max absolute gradient value (per weight).
     */
    backward(cache, targetOutput, lr, gradClip = 1.0) {
        const { inputDim, hiddenDim, outputDim, W1, b1, W2, b2 } = this;
        const { output, hidden, input } = cache;

        // ------------------------------------------------------------------
        // Output layer gradient: dL/dOutput = 2*(output - target) / outputDim
        // (MSE derivative; divide by outputDim to normalize scale)
        // ------------------------------------------------------------------
        const dOut = new Float64Array(outputDim);
        for (let k = 0; k < outputDim; k++) {
            dOut[k] = 2 * (output[k] - targetOutput[k]) / outputDim;
        }

        // ------------------------------------------------------------------
        // Gradients for W2, b2
        // ------------------------------------------------------------------
        const dW2 = new Float64Array(outputDim * hiddenDim);
        const db2 = new Float64Array(outputDim);
        for (let k = 0; k < outputDim; k++) {
            db2[k] = dOut[k];
            const rowOffset = k * hiddenDim;
            for (let j = 0; j < hiddenDim; j++) {
                dW2[rowOffset + j] = dOut[k] * hidden[j];
            }
        }

        // ------------------------------------------------------------------
        // Back-propagate through Layer 2 to get gradient at hidden layer
        // ------------------------------------------------------------------
        const dHidden = new Float64Array(hiddenDim);
        for (let j = 0; j < hiddenDim; j++) {
            let sum = 0;
            for (let k = 0; k < outputDim; k++) {
                sum += W2[k * hiddenDim + j] * dOut[k];
            }
            // ReLU derivative: pass gradient only if pre-activation was > 0.
            // We can recover the sign from hidden[j] (0 means it was ≤ 0).
            dHidden[j] = hidden[j] > 0 ? sum : 0;
        }

        // ------------------------------------------------------------------
        // Gradients for W1, b1
        // ------------------------------------------------------------------
        const dW1 = new Float64Array(hiddenDim * inputDim);
        const db1 = new Float64Array(hiddenDim);
        for (let j = 0; j < hiddenDim; j++) {
            db1[j] = dHidden[j];
            const rowOffset = j * inputDim;
            for (let i = 0; i < inputDim; i++) {
                dW1[rowOffset + i] = dHidden[j] * input[i];
            }
        }

        // ------------------------------------------------------------------
        // SGD update with gradient clipping
        // ------------------------------------------------------------------
        function clip(g) {
            return Math.max(-gradClip, Math.min(gradClip, g));
        }

        for (let i = 0; i < W1.length; i++) W1[i] -= lr * clip(dW1[i]);
        for (let i = 0; i < b1.length; i++) b1[i] -= lr * clip(db1[i]);
        for (let i = 0; i < W2.length; i++) W2[i] -= lr * clip(dW2[i]);
        for (let i = 0; i < b2.length; i++) b2[i] -= lr * clip(db2[i]);
    }

    // -----------------------------------------------------------------------
    // Weight copy
    // -----------------------------------------------------------------------

    /**
     * Copy all weights and biases from `other` into this network.
     * Used to sync the target network from the main network.
     * @param {MLP} other
     */
    copyFrom(other) {
        this.W1.set(other.W1);
        this.b1.set(other.b1);
        this.W2.set(other.W2);
        this.b2.set(other.b2);
    }
}

// ---------------------------------------------------------------------------
// ReplayBuffer — fixed-capacity ring buffer (internal, not exported)
// ---------------------------------------------------------------------------

class ReplayBuffer {
    /**
     * @param {number} capacity  Maximum number of experiences to store.
     */
    constructor(capacity) {
        this._capacity = capacity;
        this._buffer   = new Array(capacity);
        this._head     = 0;   // next write position
        this._count    = 0;   // number of stored experiences
    }

    /**
     * Add an experience to the buffer (overwrites oldest when full).
     * @param {{ state: Float64Array, action: number, reward: number,
     *           nextState: Float64Array, done: boolean }} experience
     */
    push(experience) {
        this._buffer[this._head] = experience;
        this._head = (this._head + 1) % this._capacity;
        if (this._count < this._capacity) this._count++;
    }

    /**
     * Sample `batchSize` experiences uniformly at random (with replacement).
     * @param {number} batchSize
     * @returns {Array}  Array of experience objects.
     */
    sample(batchSize) {
        const batch = new Array(batchSize);
        for (let i = 0; i < batchSize; i++) {
            const idx = Math.floor(Math.random() * this._count);
            batch[i]  = this._buffer[idx];
        }
        return batch;
    }

    /** Number of experiences currently stored. */
    get size() {
        return this._count;
    }
}

// ---------------------------------------------------------------------------
// NeuralQ — exported class
// ---------------------------------------------------------------------------

export class NeuralQ {
    /**
     * @param {object} opts
     * @param {string[]} [opts.featureKeys]         Feature keys to use (default: all)
     * @param {number}   [opts.hiddenDim=20]         Hidden layer size
     * @param {number}   [opts.alpha=0.001]          Learning rate
     * @param {number}   [opts.gamma=0.99]           Discount factor
     * @param {number}   [opts.epsilon=1.0]          Initial exploration rate
     * @param {number}   [opts.epsilonDecay=0.995]   Per-episode epsilon multiplier
     * @param {number}   [opts.epsilonMin=0.01]      Minimum epsilon
     * @param {number}   [opts.replayCapacity=2000]  Replay buffer capacity
     * @param {number}   [opts.batchSize=32]         Mini-batch size for training
     * @param {number}   [opts.targetUpdateFreq=50]  Episodes between target net syncs
     */
    constructor({
        featureKeys      = DEFAULT_FEATURE_KEYS,
        hiddenDim        = 20,
        alpha            = 0.001,
        gamma            = 0.99,
        epsilon          = 1.0,
        epsilonDecay     = 0.995,
        epsilonMin       = 0.01,
        replayCapacity   = 2000,
        batchSize        = 32,
        targetUpdateFreq = 50,
    } = {}) {
        this.featureKeys      = featureKeys;
        this.alpha            = alpha;
        this.gamma            = gamma;
        this.epsilon          = epsilon;
        this.epsilonDecay     = epsilonDecay;
        this.epsilonMin       = epsilonMin;
        this.batchSize        = batchSize;
        this.targetUpdateFreq = targetUpdateFreq;

        // Grid has 4 actions (UP, DOWN, LEFT, RIGHT).
        this._nActions = 4;

        // Main network (trained every step).
        this._net = new MLP(featureKeys.length, hiddenDim, this._nActions);

        // Target network (parameters lag behind; used for stable TD targets).
        this._target = new MLP(featureKeys.length, hiddenDim, this._nActions);
        this._target.copyFrom(this._net);

        this._replay   = new ReplayBuffer(replayCapacity);
        this._episodes = 0;
    }

    // -----------------------------------------------------------------------
    // Public properties
    // -----------------------------------------------------------------------

    get name()     { return 'Neural Q'; }
    get episodes() { return this._episodes; }

    // -----------------------------------------------------------------------
    // Feature extraction (strips the bias term appended by extractFeatures)
    // -----------------------------------------------------------------------

    /**
     * Extract features for the current grid state and strip the trailing bias
     * term that extractFeatures() appends.  The MLP has its own internal
     * biases (b1, b2), so the feature-level bias is redundant and would cause
     * an inputDim mismatch.
     *
     * @param {import('../grid.js').Grid} grid
     * @returns {Float64Array}  Length featureKeys.length (no bias term)
     */
    _extractNetFeatures(grid) {
        const raw = extractFeatures(grid, this.featureKeys);
        // raw has length featureKeys.length + 1; drop the last element (bias).
        return new Float64Array(raw.slice(0, this.featureKeys.length));
    }

    // -----------------------------------------------------------------------
    // Action selection
    // -----------------------------------------------------------------------

    /**
     * Epsilon-greedy action selection using pre-extracted net features.
     * Used internally by trainEpisode to avoid double extraction.
     *
     * @param {Float64Array} features     Pre-extracted net feature vector (no bias).
     * @param {number[]}     legalActions Legal action indices.
     * @returns {number}  Selected action index.
     */
    _selectActionFromFeatures(features, legalActions) {
        const { output } = this._net.forward(features);
        let bestAction = legalActions[0];
        let bestQ      = output[bestAction];
        for (let i = 1; i < legalActions.length; i++) {
            const q = output[legalActions[i]];
            if (q > bestQ) { bestQ = q; bestAction = legalActions[i]; }
        }
        return bestAction;
    }

    /**
     * Epsilon-greedy action selection using the main Q-network.
     *
     * @param {import('../grid.js').Grid} grid          Current environment state.
     * @param {number[]}                  legalActions  Legal action indices.
     * @returns {number}  Selected action index.
     */
    selectAction(grid, legalActions) {
        if (Math.random() < this.epsilon) {
            return legalActions[Math.floor(Math.random() * legalActions.length)];
        }
        const features = this._extractNetFeatures(grid);
        return this._selectActionFromFeatures(features, legalActions);
    }

    // -----------------------------------------------------------------------
    // Training
    // -----------------------------------------------------------------------

    /**
     * Sample a mini-batch from the replay buffer and perform one gradient step
     * on the main network using TD targets from the TARGET network.
     *
     * Does nothing if the buffer has fewer experiences than batchSize.
     */
    _trainBatch() {
        if (this._replay.size < this.batchSize) return;

        const batch = this._replay.sample(this.batchSize);

        for (const exp of batch) {
            const { state, action, reward, nextState, done } = exp;

            // Forward pass through main network for current state.
            const cache = this._net.forward(state);

            // Compute TD target using TARGET network for next-state max Q.
            let target;
            if (done) {
                target = reward;
            } else {
                const { output: nextQ } = this._target.forward(nextState);
                let maxNextQ = nextQ[0];
                for (let a = 1; a < this._nActions; a++) {
                    if (nextQ[a] > maxNextQ) maxNextQ = nextQ[a];
                }
                target = reward + this.gamma * maxNextQ;
            }

            // Build full target vector: only update the taken action's output.
            // Other outputs stay at current predicted values → zero gradient.
            const targetVec = new Float64Array(cache.output);
            targetVec[action] = target;

            this._net.backward(cache, targetVec, this.alpha);
        }
    }

    /**
     * End-of-episode bookkeeping: decay epsilon; sync target net every
     * targetUpdateFreq episodes.
     */
    endEpisode() {
        this._episodes++;
        this.epsilon = Math.max(this.epsilonMin, this.epsilon * this.epsilonDecay);
        if (this._episodes % this.targetUpdateFreq === 0) {
            this._target.copyFrom(this._net);
        }
    }

    /**
     * Run one full training episode on the given grid.
     *
     * @param {import('../grid.js').Grid} grid
     * @param {number} [maxSteps=200]  Maximum steps per episode.
     * @returns {{ totalReward: number, steps: number }}
     */
    trainEpisode(grid, maxSteps = 200) {
        grid.reset();

        let totalReward = 0;
        let steps       = 0;
        let done        = false;

        let state        = this._extractNetFeatures(grid);
        let legalActions = grid.getLegalActions();

        while (!done && steps < maxSteps) {
            const action = Math.random() < this.epsilon
                ? legalActions[Math.floor(Math.random() * legalActions.length)]
                : this._selectActionFromFeatures(state, legalActions);

            const { reward, done: isDone } = grid.step(action);

            const nextState = this._extractNetFeatures(grid);

            this._replay.push({
                state:     state,
                action:    action,
                reward:    reward,
                nextState: nextState,
                done:      isDone,
            });

            this._trainBatch();

            totalReward  += reward;
            steps++;
            done          = isDone;
            state         = nextState;
            legalActions  = grid.getLegalActions();
        }

        this.endEpisode();
        return { totalReward, steps };
    }

    // -----------------------------------------------------------------------
    // Value map (for renderer heatmap overlay)
    // -----------------------------------------------------------------------

    /**
     * Build a value map for the renderer by probing each non-wall cell.
     *
     * For each floor cell, temporarily places the agent there, extracts
     * features, runs a forward pass through the main network, and records
     * the max Q-value.  Normalizes all values to [0, 1].
     *
     * @param {import('../grid.js').Grid} grid
     * @returns {Map<string, number>}  stateKey ("x,y,bitmask") → normalizedValue
     */
    getValueMap(grid) {
        const bitmask = grid.bitmask;

        const raw = new Map();

        for (let y = 0; y < grid.height; y++) {
            for (let x = 0; x < grid.width; x++) {
                if (grid.isWall(x, y)) continue;

                const features = grid.withPosition(x, y, () => this._extractNetFeatures(grid));
                const { output } = this._net.forward(features);

                let maxQ = output[0];
                for (let a = 1; a < this._nActions; a++) {
                    if (output[a] > maxQ) maxQ = output[a];
                }

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
// selfTest (async)
// ---------------------------------------------------------------------------

/**
 * Async self-test for NeuralQ.
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
    // Shared minimal layout: 5x5, 1 item, border walls only.
    // -----------------------------------------------------------------------
    function makeTestLayout() {
        const W = 5, H = 5;
        const walls = new Set();
        for (let x = 0; x < W; x++) { walls.add(`${x},0`); walls.add(`${x},${H-1}`); }
        for (let y = 0; y < H; y++) { walls.add(`0,${y}`); walls.add(`${W-1},${y}`); }
        return {
            width:  W,
            height: H,
            walls,
            items:  [{ x: 3, y: 1 }],
            exit:   { x: 3, y: 3 },
            start:  { x: 1, y: 1 },
        };
    }

    // -----------------------------------------------------------------------
    // MLP unit tests (access via closure)
    // -----------------------------------------------------------------------

    test('MLP: forward returns correct shape', () => {
        const net = new MLP(4, 8, 3);
        const { output, hidden, input } = net.forward(new Float64Array([1, 0, -1, 0.5]));
        assert(output.length === 3, `output length should be 3, got ${output.length}`);
        assert(hidden.length === 8, `hidden length should be 8, got ${hidden.length}`);
        assert(input.length  === 4, `input length should be 4, got ${input.length}`);
    });

    test('MLP: ReLU ensures hidden units are non-negative', () => {
        const net = new MLP(4, 16, 3);
        const { hidden } = net.forward(new Float64Array([1, -1, 1, -1]));
        for (let i = 0; i < hidden.length; i++) {
            assert(hidden[i] >= 0, `hidden[${i}] = ${hidden[i]} < 0 (ReLU violation)`);
        }
    });

    test('MLP: backward changes weights', () => {
        const net = new MLP(3, 4, 2);
        const w1Before = net.W1.slice();
        const input    = new Float64Array([0.5, -0.3, 0.8]);
        const cache    = net.forward(input);
        const target   = new Float64Array([1.0, -1.0]);
        net.backward(cache, target, 0.01);
        let changed = false;
        for (let i = 0; i < net.W1.length; i++) {
            if (net.W1[i] !== w1Before[i]) { changed = true; break; }
        }
        assert(changed, 'backward should update W1 weights');
    });

    test('MLP: copyFrom produces identical outputs', () => {
        const src = new MLP(3, 5, 2);
        const dst = new MLP(3, 5, 2);
        dst.copyFrom(src);
        const inp  = new Float64Array([0.1, 0.2, 0.3]);
        const outA = src.forward(inp).output;
        const outB = dst.forward(inp).output;
        for (let k = 0; k < outA.length; k++) {
            assert(outA[k] === outB[k], `output mismatch at ${k}: ${outA[k]} vs ${outB[k]}`);
        }
    });

    // -----------------------------------------------------------------------
    // ReplayBuffer unit tests
    // -----------------------------------------------------------------------

    test('ReplayBuffer: size tracks pushes up to capacity', () => {
        const rb = new ReplayBuffer(5);
        assert(rb.size === 0, 'empty buffer size should be 0');
        for (let i = 0; i < 3; i++) rb.push({ i });
        assert(rb.size === 3, `size should be 3, got ${rb.size}`);
        for (let i = 0; i < 4; i++) rb.push({ i }); // overflow capacity
        assert(rb.size === 5, `size should cap at 5, got ${rb.size}`);
    });

    test('ReplayBuffer: sample returns correct batch size', () => {
        const rb = new ReplayBuffer(100);
        for (let i = 0; i < 50; i++) rb.push({ v: i });
        const batch = rb.sample(10);
        assert(batch.length === 10, `batch length should be 10, got ${batch.length}`);
    });

    // -----------------------------------------------------------------------
    // NeuralQ unit tests
    // -----------------------------------------------------------------------

    test('NeuralQ: constructor defaults', () => {
        const q = new NeuralQ();
        assert(q.name     === 'Neural Q', `name: ${q.name}`);
        assert(q.episodes === 0,          `episodes: ${q.episodes}`);
        assert(q.epsilon  === 1.0,        `epsilon: ${q.epsilon}`);
    });

    test('NeuralQ: selectAction returns valid action (explore)', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({ epsilon: 1.0 }); // always explore
        const a    = q.selectAction(grid, grid.getLegalActions());
        assert([0, 1, 2, 3].includes(a), `invalid action: ${a}`);
    });

    test('NeuralQ: selectAction returns valid action (exploit)', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({ epsilon: 0.0 }); // always exploit
        const a    = q.selectAction(grid, grid.getLegalActions());
        assert([0, 1, 2, 3].includes(a), `invalid action: ${a}`);
    });

    test('NeuralQ: trainEpisode returns totalReward and steps', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({ replayCapacity: 50, batchSize: 8 });
        const { totalReward, steps } = q.trainEpisode(grid);
        assert(typeof totalReward === 'number', 'totalReward should be a number');
        assert(typeof steps       === 'number', 'steps should be a number');
        assert(steps > 0   && steps <= 200,     `steps out of range: ${steps}`);
    });

    test('NeuralQ: trainEpisode increments episode counter', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({ replayCapacity: 50, batchSize: 8 });
        assert(q.episodes === 0, 'should start at 0');
        q.trainEpisode(grid);
        assert(q.episodes === 1, `after 1: ${q.episodes}`);
        q.trainEpisode(grid);
        assert(q.episodes === 2, `after 2: ${q.episodes}`);
    });

    test('NeuralQ: endEpisode decays epsilon', () => {
        const q = new NeuralQ({ epsilon: 1.0, epsilonDecay: 0.5, epsilonMin: 0.01 });
        q.endEpisode();
        assert(Math.abs(q.epsilon - 0.5) < 1e-9, `epsilon after 1 decay: ${q.epsilon}`);
        q.endEpisode();
        assert(Math.abs(q.epsilon - 0.25) < 1e-9, `epsilon after 2 decays: ${q.epsilon}`);
    });

    test('NeuralQ: epsilon does not go below epsilonMin', () => {
        const q = new NeuralQ({ epsilon: 0.011, epsilonDecay: 0.5, epsilonMin: 0.01 });
        q.endEpisode();
        assert(q.epsilon === 0.01, `epsilon should be 0.01, got ${q.epsilon}`);
    });

    test('NeuralQ: getValueMap returns Map with [0,1] values', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({ replayCapacity: 50, batchSize: 8 });
        // Train briefly so outputs are not all the same constant.
        for (let i = 0; i < 5; i++) q.trainEpisode(grid);
        const vm = q.getValueMap(grid);
        assert(vm instanceof Map, 'should return a Map');
        for (const [key, val] of vm) {
            assert(val >= 0 && val <= 1, `value ${val} for ${key} out of [0,1]`);
        }
    });

    test('NeuralQ: target network sync at targetUpdateFreq', () => {
        const grid = new Grid(makeTestLayout());
        const q    = new NeuralQ({
            targetUpdateFreq: 3,
            replayCapacity:   50,
            batchSize:        8,
            alpha:            0.1, // larger lr so weights change visibly
        });
        // Train 2 episodes — target should NOT yet be synced to main net.
        for (let i = 0; i < 2; i++) q.trainEpisode(grid);
        const inp     = new Float64Array(DEFAULT_FEATURE_KEYS.length).fill(0.5);
        const mainOut = q._net.forward(inp).output;
        const tgtOut  = q._target.forward(inp).output;
        // After only 2 episodes they may or may not differ — just check types.
        assert(mainOut.length === 4, 'main net output should have 4 actions');
        assert(tgtOut.length  === 4, 'target net output should have 4 actions');

        // Train 1 more episode → 3rd call → sync should fire.
        q.trainEpisode(grid);
        assert(q.episodes === 3, `episodes should be 3, got ${q.episodes}`);
        // After sync, target and main outputs should match.
        const mainOut2 = q._net.forward(inp).output;
        const tgtOut2  = q._target.forward(inp).output;
        for (let k = 0; k < 4; k++) {
            assert(
                Math.abs(mainOut2[k] - tgtOut2[k]) < 1e-12,
                `after sync: main[${k}]=${mainOut2[k]} vs target[${k}]=${tgtOut2[k]}`
            );
        }
    });

    // -----------------------------------------------------------------------
    // Integration test: train 300 episodes, check convergence and diversity
    // -----------------------------------------------------------------------
    await (async () => {
        const name = 'integration: 300 episodes on 5x5, avg reward last 50 > 1, outputs not identical';
        try {
            const grid = new Grid(makeTestLayout());
            const q    = new NeuralQ({
                hiddenDim:        16,
                alpha:            0.005,
                gamma:            0.99,
                epsilon:          1.0,
                epsilonDecay:     0.99,
                epsilonMin:       0.01,
                replayCapacity:   500,
                batchSize:        16,
                targetUpdateFreq: 20,
            });

            const rewardHistory = [];
            for (let i = 0; i < 300; i++) {
                const { totalReward } = q.trainEpisode(grid);
                rewardHistory.push(totalReward);
            }

            const last50 = rewardHistory.slice(-50);
            const avg    = last50.reduce((s, v) => s + v, 0) / last50.length;

            assert(avg > 1,
                `avg reward last 50 episodes should be > 1, got ${avg.toFixed(3)}`);

            // Check that network outputs are not all identical across different positions.
            const vals = new Set();
            for (let x = 1; x < 4; x++) {
                for (let y = 1; y < 4; y++) {
                    if (grid.isWall(x, y)) continue;
                    const feats = grid.withPosition(x, y, () => q._extractNetFeatures(grid));
                    const { output } = q._net.forward(feats);
                    vals.add(output[0].toFixed(4));
                }
            }
            assert(vals.size > 1, 'network outputs should vary across positions');

            results.push({
                name,
                passed:  true,
                message: `avg=${avg.toFixed(3)}, distinct outputs=${vals.size}`,
            });
        } catch (e) {
            results.push({ name, passed: false, message: e.message });
        }
    })();

    return results;
}
