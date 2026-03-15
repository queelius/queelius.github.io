/**
 * widgets.js — Orchestration layer wiring grid, renderer, controls, and
 * algorithms into interactive widgets for the blog posts.
 *
 * Exports:
 *   createPlayWidget(container, opts)         — manual arrow-key control
 *   createTrainWidget(container, opts)        — live agent training with heatmap
 *   createCompareWidget(container, opts)      — multi-agent side-by-side comparison
 *   createFeatureExplorerWidget(container, opts) — linear Q with toggleable features
 */

import { Grid, generateLayout, Actions } from './grid.js';
import { GridRenderer } from './render.js';
import { createSlider, createPlaybackControls, createStats } from './controls.js';
import { FEATURES, DEFAULT_FEATURE_KEYS } from './features.js';
import { LinearQ } from './algorithms/linear-q.js';

// ---------------------------------------------------------------------------
// Helper: count set bits in a number (popcount)
// ---------------------------------------------------------------------------

function countBits(n) {
    let count = 0;
    let v = n >>> 0;
    while (v) {
        count += v & 1;
        v >>>= 1;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Helper: compute rolling average over last N values
// ---------------------------------------------------------------------------

function rollingAvg(history, n) {
    if (history.length === 0) return 0;
    const slice = history.slice(-n);
    return slice.reduce((s, v) => s + v, 0) / slice.length;
}

// ---------------------------------------------------------------------------
// Helper: create a wall density selector (dropdown)
// ---------------------------------------------------------------------------

function createWallDensitySelect(currentValue, onChange) {
    const label = document.createElement('label');
    label.style.display = 'flex';
    label.style.alignItems = 'center';
    label.style.gap = '0.35rem';
    label.style.color = 'var(--muted)';
    label.style.fontFamily = 'system-ui, sans-serif';
    label.style.fontSize = '0.85rem';

    const span = document.createElement('span');
    span.textContent = 'Walls:';

    const select = document.createElement('select');
    select.style.fontSize = '0.85rem';
    select.style.padding = '0.2rem 0.4rem';
    select.style.border = '1px solid var(--border)';
    select.style.borderRadius = '4px';
    select.style.background = 'var(--bg)';
    select.style.color = 'var(--fg)';

    for (const val of ['none', 'sparse', 'maze']) {
        const opt = document.createElement('option');
        opt.value = val;
        opt.textContent = val;
        if (val === currentValue) opt.selected = true;
        select.appendChild(opt);
    }

    select.addEventListener('change', () => onChange(select.value));

    label.appendChild(span);
    label.appendChild(select);
    return label;
}

// ---------------------------------------------------------------------------
// Helper: build shared config controls for train/compare/feature widgets
// Returns { element, config } where config is mutable and onChange fires on
// any change that requires a grid/agent rebuild.
// ---------------------------------------------------------------------------

function createConfigControls({ gridSize, nItems, wallDensity, speed, onRebuild, onSpeedChange }) {
    const wrapper = document.createElement('div');
    wrapper.className = 'widget-controls';

    const config = {
        gridSize,
        nItems,
        wallDensity,
        speed,
    };

    const sizeSlider = createSlider({
        label: 'Grid size',
        min: 6,
        max: 20,
        value: gridSize,
        onChange(v) {
            config.gridSize = v;
            onRebuild();
        },
    });

    const itemSlider = createSlider({
        label: 'Items',
        min: 1,
        max: 6,
        value: nItems,
        onChange(v) {
            config.nItems = v;
            onRebuild();
        },
    });

    const wallSelect = createWallDensitySelect(wallDensity, (v) => {
        config.wallDensity = v;
        onRebuild();
    });

    const speedSlider = createSlider({
        label: 'Speed',
        min: 1,
        max: 100,
        value: speed,
        onChange(v) {
            config.speed = v;
            if (onSpeedChange) onSpeedChange(v);
        },
    });

    wrapper.appendChild(sizeSlider);
    wrapper.appendChild(itemSlider);
    wrapper.appendChild(wallSelect);
    wrapper.appendChild(speedSlider);

    return { element: wrapper, config };
}

// ---------------------------------------------------------------------------
// 1. createPlayWidget
// ---------------------------------------------------------------------------

/**
 * Manual play widget — arrow keys move the agent.
 *
 * @param {HTMLElement} container  DOM element to mount into
 * @param {object}      opts
 * @param {number}      [opts.gridSize=8]         Grid width/height
 * @param {number}      [opts.nItems=3]           Number of collectible items
 * @param {string}      [opts.wallDensity='sparse'] Wall density: none/sparse/maze
 * @param {number}      [opts.seed=42]            Layout seed
 * @returns {{ destroy(): void }}
 */
export function createPlayWidget(container, opts = {}) {
    const {
        gridSize    = 8,
        nItems      = 3,
        wallDensity = 'sparse',
        seed        = 42,
    } = opts;

    // ---- State ----
    let grid;
    let renderer;
    let steps       = 0;
    let totalReward = 0;

    // ---- Build DOM ----
    const widget = document.createElement('div');
    widget.className = 'widget';

    const canvas = document.createElement('canvas');
    canvas.tabIndex = 0;  // make focusable
    canvas.style.cursor = 'pointer';
    canvas.title = 'Click to focus, then use arrow keys to move';

    // Controls row
    const controls = document.createElement('div');
    controls.className = 'widget-controls';

    const resetBtn = document.createElement('button');
    resetBtn.textContent = 'Reset';
    resetBtn.addEventListener('click', () => {
        doReset();
        canvas.focus();
    });

    controls.appendChild(resetBtn);

    const focusHint = document.createElement('span');
    focusHint.textContent = 'Click canvas to focus, then use arrow keys';
    focusHint.style.color = 'var(--muted)';
    focusHint.style.fontSize = '0.8rem';
    controls.appendChild(focusHint);

    const stats = createStats();

    widget.appendChild(canvas);
    widget.appendChild(controls);
    widget.appendChild(stats.element);
    container.appendChild(widget);

    // ---- Initialise grid & renderer ----
    function init() {
        const layout = generateLayout({ width: gridSize, height: gridSize, nItems, wallDensity, seed });
        grid     = new Grid(layout);
        if (renderer) {
            renderer.resize(grid);
        } else {
            renderer = new GridRenderer(canvas, grid);
        }
    }

    function doReset() {
        grid.reset();
        renderer.clearTrail();
        steps       = 0;
        totalReward = 0;
        updateStats();
        renderer.render();
    }

    function updateStats(done = false) {
        const state     = grid.getState();
        const collected = grid.items.length - countBits(state.bitmask);
        const total     = grid.items.length;
        let text = `Steps: ${steps}  Reward: ${totalReward.toFixed(2)}  Items: ${collected}/${total}`;
        if (done) text += '  DONE!';
        stats.update(text);
    }

    // ---- Keyboard handler ----
    const KEY_MAP = {
        ArrowUp:    Actions.UP,
        ArrowDown:  Actions.DOWN,
        ArrowLeft:  Actions.LEFT,
        ArrowRight: Actions.RIGHT,
    };

    function onKey(e) {
        const action = KEY_MAP[e.key];
        if (action === undefined) return;
        e.preventDefault();

        const state = grid.getState();
        if (grid._done) return;

        renderer.addTrailPoint(state.x, state.y);
        const { state: nextState, reward, done } = grid.step(action);

        steps++;
        totalReward += reward;
        updateStats(done);
        renderer.render();
    }

    canvas.addEventListener('keydown', onKey);

    // Focus canvas on click
    canvas.addEventListener('click', () => canvas.focus());

    // ---- Mount ----
    init();
    updateStats();
    renderer.render();

    return {
        destroy() {
            canvas.removeEventListener('keydown', onKey);
            container.removeChild(widget);
        },
    };
}

// ---------------------------------------------------------------------------
// 2. createTrainWidget
// ---------------------------------------------------------------------------

/**
 * Live training widget — agent trains with value heatmap overlay.
 *
 * @param {HTMLElement} container
 * @param {object}      opts
 * @param {function}    opts.createAgent   Factory: (grid) => agent instance with trainEpisode(grid) and getValueMap(grid)
 * @param {number}      [opts.gridSize=8]
 * @param {number}      [opts.nItems=3]
 * @param {string}      [opts.wallDensity='sparse']
 * @param {number}      [opts.seed=42]
 * @param {number}      [opts.speed=10]    Episodes per animation frame
 * @returns {{ destroy(): void, getAgent(): object }}
 */
export function createTrainWidget(container, opts = {}) {
    const {
        createAgent,
        gridSize    = 8,
        nItems      = 3,
        wallDensity = 'sparse',
        seed        = 42,
        speed: initialSpeed = 10,
    } = opts;

    // ---- Mutable config (sliders write here, rebuild reads here) ----
    const config = {
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
    };

    // ---- State ----
    let grid;
    let agent;
    let renderer;
    let rewardHistory = [];
    let playing       = false;
    let rafId         = null;
    let watching      = false;
    let watchStopped  = false;
    let currentSeed   = seed;

    // ---- Build DOM ----
    const widget = document.createElement('div');
    widget.className = 'widget';

    const canvas = document.createElement('canvas');

    const configControls = createConfigControls({
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
        onRebuild() {
            stopWatch();
            // Config object is already updated by the sliders' onChange callbacks.
            // Sync our local config from the control's config object.
            config.gridSize    = configControls.config.gridSize;
            config.nItems      = configControls.config.nItems;
            config.wallDensity = configControls.config.wallDensity;
            rebuild();
        },
        onSpeedChange(v) {
            config.speed = v;
        },
    });

    const playback = createPlaybackControls({
        onStep() {
            if (!agent || watching) return;
            const { totalReward } = agent.trainEpisode(grid);
            rewardHistory.push(totalReward);
            if (rewardHistory.length > 200) rewardHistory.shift();
            updateValueMap();
            renderer.render();
            updateStats();
        },
        onPlayPause(isPlaying) {
            if (watching) return;
            playing = isPlaying;
            if (playing) {
                scheduleFrame();
            } else {
                if (rafId !== null) {
                    cancelAnimationFrame(rafId);
                    rafId = null;
                }
            }
        },
        onReset() {
            stopWatch();
            playing = false;
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
            currentSeed = seed;
            rebuild();
        },
    });

    // Extra controls row: Watch + New Layout
    const extraControls = document.createElement('div');
    extraControls.className = 'widget-controls';

    const watchBtn = document.createElement('button');
    watchBtn.textContent = 'Watch';
    watchBtn.title = 'Watch the trained agent play (pure exploitation)';
    watchBtn.addEventListener('click', () => {
        if (watching) {
            stopWatch();
        } else {
            startWatch();
        }
    });

    const newLayoutBtn = document.createElement('button');
    newLayoutBtn.textContent = 'New Layout';
    newLayoutBtn.title = 'Generate a new grid layout (keeps trained weights)';
    newLayoutBtn.addEventListener('click', () => {
        stopWatch();
        currentSeed++;
        grid = buildGrid();
        renderer.resize(grid);
        renderer.clearTrail();
        renderer.setValueMap(null);
        renderer.render();
        updateStats();
    });

    extraControls.appendChild(watchBtn);
    extraControls.appendChild(newLayoutBtn);

    const stats = createStats();

    widget.appendChild(canvas);
    widget.appendChild(configControls.element);
    widget.appendChild(playback.element);
    widget.appendChild(extraControls);
    widget.appendChild(stats.element);
    container.appendChild(widget);

    // ---- Helpers ----
    function buildGrid() {
        const layout = generateLayout({
            width:       config.gridSize,
            height:      config.gridSize,
            nItems:      config.nItems,
            wallDensity: config.wallDensity,
            seed:        currentSeed,
        });
        return new Grid(layout);
    }

    function updateValueMap() {
        if (!agent || typeof agent.getValueMap !== 'function') return;
        // Reset grid so the heatmap shows the initial bitmask (where most
        // Q-values live).  Without this, tabular agents show an almost-empty
        // map because the grid is left at bitmask=0 after each episode.
        grid.reset();
        const vm = agent.getValueMap(grid);
        renderer.setValueMap(vm.size > 0 ? vm : null);
    }

    function updateStats() {
        if (!agent) {
            stats.update('No agent');
            return;
        }
        const agentName  = agent.name ?? 'Agent';
        const episodes   = agent.episodes ?? 0;
        const stateCount = grid.stateCount();
        const avgReward  = rollingAvg(rewardHistory, 100).toFixed(3);
        const epsilon    = agent.epsilon !== undefined ? agent.epsilon.toFixed(4) : 'n/a';
        stats.update(
            `${agentName}  |  Episodes: ${episodes}  States: ${stateCount}  ` +
            `Avg reward (100): ${avgReward}  ε: ${epsilon}`
        );
    }

    function rebuild() {
        grid          = buildGrid();
        agent         = createAgent ? createAgent(grid) : null;
        rewardHistory = [];
        if (renderer) {
            renderer.resize(grid);
            renderer.setValueMap(null);
            renderer.clearTrail();
        } else {
            renderer = new GridRenderer(canvas, grid);
        }
        renderer.render();
        updateStats();
    }

    function scheduleFrame() {
        rafId = requestAnimationFrame(frame);
    }

    function frame() {
        if (!playing) return;
        if (agent) {
            for (let i = 0; i < config.speed; i++) {
                const { totalReward } = agent.trainEpisode(grid);
                rewardHistory.push(totalReward);
                if (rewardHistory.length > 200) rewardHistory.shift();
            }
            updateValueMap();
            renderer.render();
            updateStats();
        }
        scheduleFrame();
    }

    // ---- Watch mode ----
    function stopWatch() {
        if (!watching) return;
        watchStopped = true;
        watching = false;
        watchBtn.textContent = 'Watch';
        playback.setPlaying(false);
    }

    async function startWatch() {
        if (!agent) return;
        // Stop training loop if running
        if (playing) {
            playing = false;
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
            playback.setPlaying(false);
        }

        watching = true;
        watchStopped = false;
        watchBtn.textContent = 'Stop';

        grid.reset();
        renderer.clearTrail();
        renderer.setValueMap(null);
        renderer.render();

        const savedEpsilon = agent.epsilon;
        if (agent.epsilon !== undefined) agent.epsilon = 0;

        let totalReward = 0;
        for (let step = 0; step < 200; step++) {
            if (watchStopped) break;
            renderer.addTrailPoint(grid.x, grid.y);
            const legalActions = grid.getLegalActions();
            const action = agent.selectAction(grid, legalActions);
            const { reward, done } = grid.step(action);
            totalReward += reward;
            renderer.render();
            stats.update(
                `Watching | Step: ${step + 1} | Reward: ${totalReward.toFixed(2)}` +
                (done ? ' | DONE!' : '')
            );
            if (done) break;
            await new Promise(r => setTimeout(r, 150));
        }

        if (agent.epsilon !== undefined) agent.epsilon = savedEpsilon;

        if (watching) {
            watching = false;
            watchBtn.textContent = 'Watch';
            playback.setPlaying(false);
        }
    }

    // ---- Mount ----
    rebuild();

    return {
        destroy() {
            stopWatch();
            playing = false;
            if (rafId !== null) cancelAnimationFrame(rafId);
            container.removeChild(widget);
        },
        getAgent() {
            return agent;
        },
    };
}

// ---------------------------------------------------------------------------
// 3. createCompareWidget
// ---------------------------------------------------------------------------

/**
 * Multi-agent comparison widget — agents train side-by-side on identical grids.
 *
 * @param {HTMLElement} container
 * @param {object}      opts
 * @param {Array<{name:string, createAgent:function}>} opts.agents  Agent definitions
 * @param {number}      [opts.gridSize=8]
 * @param {number}      [opts.nItems=3]
 * @param {string}      [opts.wallDensity='sparse']
 * @param {number}      [opts.seed=42]
 * @param {number}      [opts.speed=10]
 * @returns {{ destroy(): void }}
 */
export function createCompareWidget(container, opts = {}) {
    const {
        agents: agentDefs = [],
        gridSize    = 8,
        nItems      = 3,
        wallDensity = 'sparse',
        seed        = 42,
        speed: initialSpeed = 10,
    } = opts;

    // ---- Mutable config ----
    const config = {
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
    };

    // ---- Per-agent state ----
    let panels = [];   // [{ grid, agent, renderer, stats, rewardHistory }]
    let playing      = false;
    let rafId        = null;
    let watching     = false;
    let watchStopped = false;
    let currentSeed  = seed;

    // ---- Build DOM ----
    const widget = document.createElement('div');
    widget.className = 'widget';

    const compareRow = document.createElement('div');
    compareRow.className = 'compare-row';

    const configControls = createConfigControls({
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
        onRebuild() {
            stopWatch();
            config.gridSize    = configControls.config.gridSize;
            config.nItems      = configControls.config.nItems;
            config.wallDensity = configControls.config.wallDensity;
            rebuild();
        },
        onSpeedChange(v) {
            config.speed = v;
        },
    });

    const playback = createPlaybackControls({
        onStep() {
            if (watching) return;
            stepAll();
        },
        onPlayPause(isPlaying) {
            if (watching) return;
            playing = isPlaying;
            if (playing) {
                scheduleFrame();
            } else {
                if (rafId !== null) {
                    cancelAnimationFrame(rafId);
                    rafId = null;
                }
            }
        },
        onReset() {
            stopWatch();
            playing = false;
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
            currentSeed = seed;
            rebuild();
        },
    });

    // Extra controls row: Watch + New Layout
    const extraControls = document.createElement('div');
    extraControls.className = 'widget-controls';

    const watchBtn = document.createElement('button');
    watchBtn.textContent = 'Watch';
    watchBtn.title = 'Watch all trained agents play simultaneously (pure exploitation)';
    watchBtn.addEventListener('click', () => {
        if (watching) {
            stopWatch();
        } else {
            startWatch();
        }
    });

    const newLayoutBtn = document.createElement('button');
    newLayoutBtn.textContent = 'New Layout';
    newLayoutBtn.title = 'Generate a new grid layout for all agents (keeps trained weights)';
    newLayoutBtn.addEventListener('click', () => {
        stopWatch();
        currentSeed++;
        const layout = generateLayout({
            width:       config.gridSize,
            height:      config.gridSize,
            nItems:      config.nItems,
            wallDensity: config.wallDensity,
            seed:        currentSeed,
        });
        for (const entry of panels) {
            entry.grid = new Grid(layout);
            entry.renderer.resize(entry.grid);
            entry.renderer.clearTrail();
            entry.renderer.setValueMap(null);
            entry.renderer.render();
            updatePanelStats(entry);
        }
    });

    extraControls.appendChild(watchBtn);
    extraControls.appendChild(newLayoutBtn);

    widget.appendChild(configControls.element);
    widget.appendChild(playback.element);
    widget.appendChild(extraControls);
    widget.appendChild(compareRow);
    container.appendChild(widget);

    // ---- Helpers ----
    function buildLayout() {
        return generateLayout({
            width:       config.gridSize,
            height:      config.gridSize,
            nItems:      config.nItems,
            wallDensity: config.wallDensity,
            seed:        currentSeed,
        });
    }

    function rebuild() {
        // Wipe existing panels
        compareRow.textContent = '';
        panels = [];

        const layout = buildLayout(); // identical layout for all agents

        for (const def of agentDefs) {
            const panel = document.createElement('div');
            panel.className = 'compare-panel';

            const title = document.createElement('h4');
            title.textContent = def.name;
            panel.appendChild(title);

            const canvas = document.createElement('canvas');
            panel.appendChild(canvas);

            const statsEl = createStats();
            panel.appendChild(statsEl.element);

            compareRow.appendChild(panel);

            // Construct grid and agent from the same layout
            const grid = new Grid(layout);
            const agent = def.createAgent ? def.createAgent(grid) : null;
            const renderer = new GridRenderer(canvas, grid);
            renderer.render();

            const entry = { grid, agent, renderer, stats: statsEl, rewardHistory: [] };
            panels.push(entry);
            updatePanelStats(entry);
        }
    }

    function updatePanelStats(entry) {
        const { agent, grid, rewardHistory, stats } = entry;
        if (!agent) {
            stats.update('No agent');
            return;
        }
        const episodes  = agent.episodes ?? 0;
        const avgReward = rollingAvg(rewardHistory, 100).toFixed(3);
        const epsilon   = agent.epsilon !== undefined ? agent.epsilon.toFixed(4) : 'n/a';
        stats.update(`Ep: ${episodes}  Avg: ${avgReward}  ε: ${epsilon}`);
    }

    function stepAll() {
        for (const entry of panels) {
            const { agent, grid, renderer, rewardHistory } = entry;
            if (!agent) continue;
            const { totalReward } = agent.trainEpisode(grid);
            rewardHistory.push(totalReward);
            if (rewardHistory.length > 200) rewardHistory.shift();
            if (typeof agent.getValueMap === 'function') {
                grid.reset(); // show initial-bitmask Q-values
                const vm = agent.getValueMap(grid);
                renderer.setValueMap(vm.size > 0 ? vm : null);
            }
            renderer.render();
            updatePanelStats(entry);
        }
    }

    function scheduleFrame() {
        rafId = requestAnimationFrame(frame);
    }

    function frame() {
        if (!playing) return;
        for (let i = 0; i < config.speed; i++) {
            for (const entry of panels) {
                if (!entry.agent) continue;
                const { totalReward } = entry.agent.trainEpisode(entry.grid);
                entry.rewardHistory.push(totalReward);
                if (entry.rewardHistory.length > 200) entry.rewardHistory.shift();
            }
        }
        for (const entry of panels) {
            if (!entry.agent) continue;
            if (typeof entry.agent.getValueMap === 'function') {
                const vm = entry.agent.getValueMap(entry.grid);
                entry.renderer.setValueMap(vm.size > 0 ? vm : null);
            }
            entry.renderer.render();
            updatePanelStats(entry);
        }
        scheduleFrame();
    }

    // ---- Watch mode ----
    function stopWatch() {
        if (!watching) return;
        watchStopped = true;
        watching = false;
        watchBtn.textContent = 'Watch';
        playback.setPlaying(false);
    }

    async function startWatch() {
        if (panels.length === 0) return;
        // Stop training loop if running
        if (playing) {
            playing = false;
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
            playback.setPlaying(false);
        }

        watching = true;
        watchStopped = false;
        watchBtn.textContent = 'Stop';

        // Reset all panels
        for (const entry of panels) {
            entry.grid.reset();
            entry.renderer.clearTrail();
            entry.renderer.setValueMap(null);
            entry.renderer.render();
        }

        // Save and zero epsilons
        const savedEpsilons = panels.map(e =>
            (e.agent && e.agent.epsilon !== undefined) ? e.agent.epsilon : null
        );
        for (const entry of panels) {
            if (entry.agent && entry.agent.epsilon !== undefined) entry.agent.epsilon = 0;
        }

        const dones = panels.map(() => false);
        const totalRewards = panels.map(() => 0);

        for (let step = 0; step < 200; step++) {
            if (watchStopped) break;
            for (let i = 0; i < panels.length; i++) {
                if (dones[i]) continue;
                const { grid, agent, renderer, stats } = panels[i];
                if (!agent) { dones[i] = true; continue; }
                renderer.addTrailPoint(grid.x, grid.y);
                const legalActions = grid.getLegalActions();
                const action = agent.selectAction(grid, legalActions);
                const { reward, done } = grid.step(action);
                totalRewards[i] += reward;
                renderer.render();
                stats.update(
                    `Watching | Step: ${step + 1} | Reward: ${totalRewards[i].toFixed(2)}` +
                    (done ? ' | DONE!' : '')
                );
                if (done) dones[i] = true;
            }
            if (dones.every(Boolean)) break;
            await new Promise(r => setTimeout(r, 150));
        }

        // Restore epsilons
        for (let i = 0; i < panels.length; i++) {
            if (panels[i].agent && savedEpsilons[i] !== null) {
                panels[i].agent.epsilon = savedEpsilons[i];
            }
        }

        if (watching) {
            watching = false;
            watchBtn.textContent = 'Watch';
            playback.setPlaying(false);
        }
    }

    // ---- Mount ----
    rebuild();

    return {
        destroy() {
            stopWatch();
            playing = false;
            if (rafId !== null) cancelAnimationFrame(rafId);
            container.removeChild(widget);
        },
    };
}

// ---------------------------------------------------------------------------
// 4. createFeatureExplorerWidget
// ---------------------------------------------------------------------------

/**
 * Feature explorer — linear Q-learning with toggleable features.
 *
 * @param {HTMLElement} container
 * @param {object}      opts
 * @param {number}      [opts.gridSize=8]
 * @param {number}      [opts.nItems=3]
 * @param {string}      [opts.wallDensity='sparse']
 * @param {number}      [opts.seed=42]
 * @param {number}      [opts.speed=10]
 * @returns {{ destroy(): void }}
 */
export function createFeatureExplorerWidget(container, opts = {}) {
    const {
        gridSize    = 8,
        nItems      = 3,
        wallDensity = 'sparse',
        seed        = 42,
        speed: initialSpeed = 10,
    } = opts;

    // ---- Mutable config ----
    const config = {
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
    };

    // ---- Active feature keys ----
    let activeKeys = [...DEFAULT_FEATURE_KEYS];

    // ---- State ----
    let grid;
    let agent;
    let renderer;
    let rewardHistory = [];
    let playing       = false;
    let rafId         = null;

    // ---- Build DOM ----
    const widget = document.createElement('div');
    widget.className = 'widget';

    const canvas = document.createElement('canvas');

    // Feature toggles
    const featureToggles = document.createElement('div');
    featureToggles.className = 'feature-toggles';

    for (const feature of FEATURES) {
        const label = document.createElement('label');

        const checkbox = document.createElement('input');
        checkbox.type    = 'checkbox';
        checkbox.value   = feature.key;
        checkbox.checked = DEFAULT_FEATURE_KEYS.includes(feature.key);
        checkbox.title   = feature.description;

        checkbox.addEventListener('change', () => {
            if (checkbox.checked) {
                if (!activeKeys.includes(feature.key)) {
                    // Insert in original FEATURES order
                    const allKeys = FEATURES.map(f => f.key);
                    activeKeys = allKeys.filter(k =>
                        k === feature.key || activeKeys.includes(k)
                    );
                }
            } else {
                activeKeys = activeKeys.filter(k => k !== feature.key);
            }
            rebuild();
        });

        const nameSpan = document.createElement('span');
        nameSpan.textContent = feature.name;

        label.appendChild(checkbox);
        label.appendChild(nameSpan);
        featureToggles.appendChild(label);
    }

    const configControls = createConfigControls({
        gridSize,
        nItems,
        wallDensity,
        speed: initialSpeed,
        onRebuild() {
            stopWatch();
            config.gridSize    = configControls.config.gridSize;
            config.nItems      = configControls.config.nItems;
            config.wallDensity = configControls.config.wallDensity;
            rebuild();
        },
        onSpeedChange(v) {
            config.speed = v;
        },
    });

    const playback = createPlaybackControls({
        onStep() {
            if (!agent) return;
            const { totalReward } = agent.trainEpisode(grid);
            rewardHistory.push(totalReward);
            if (rewardHistory.length > 200) rewardHistory.shift();
            updateValueMap();
            renderer.render();
            updateStats();
        },
        onPlayPause(isPlaying) {
            playing = isPlaying;
            if (playing) {
                scheduleFrame();
            } else {
                if (rafId !== null) {
                    cancelAnimationFrame(rafId);
                    rafId = null;
                }
            }
        },
        onReset() {
            playing = false;
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }
            rebuild();
        },
    });

    const stats = createStats();

    widget.appendChild(canvas);
    widget.appendChild(featureToggles);
    widget.appendChild(configControls.element);
    widget.appendChild(playback.element);
    widget.appendChild(stats.element);
    container.appendChild(widget);

    // ---- Helpers ----
    function buildGrid() {
        const layout = generateLayout({
            width:       config.gridSize,
            height:      config.gridSize,
            nItems:      config.nItems,
            wallDensity: config.wallDensity,
            seed,
        });
        return new Grid(layout);
    }

    function updateValueMap() {
        if (!agent) return;
        grid.reset(); // show initial-bitmask Q-values
        const vm = agent.getValueMap(grid);
        renderer.setValueMap(vm.size > 0 ? vm : null);
    }

    function updateStats() {
        const featureCount = activeKeys.length;
        const episodes     = agent ? (agent.episodes ?? 0) : 0;
        const avgReward    = rollingAvg(rewardHistory, 100).toFixed(3);
        stats.update(
            `Linear Q  |  Features: ${featureCount}  Episodes: ${episodes}  Avg reward (100): ${avgReward}`
        );
    }

    function rebuild() {
        playing = false;
        if (rafId !== null) {
            cancelAnimationFrame(rafId);
            rafId = null;
        }
        playback.setPlaying(false);

        grid          = buildGrid();
        rewardHistory = [];

        // Use activeKeys; fall back to a single bias-only agent if none selected
        const keys = activeKeys.length > 0 ? activeKeys : [];
        agent = new LinearQ({ featureKeys: keys });

        if (renderer) {
            renderer.resize(grid);
            renderer.setValueMap(null);
            renderer.clearTrail();
        } else {
            renderer = new GridRenderer(canvas, grid);
        }
        renderer.render();
        updateStats();
    }

    function scheduleFrame() {
        rafId = requestAnimationFrame(frame);
    }

    function frame() {
        if (!playing) return;
        for (let i = 0; i < config.speed; i++) {
            const { totalReward } = agent.trainEpisode(grid);
            rewardHistory.push(totalReward);
            if (rewardHistory.length > 200) rewardHistory.shift();
        }
        updateValueMap();
        renderer.render();
        updateStats();
        scheduleFrame();
    }

    // ---- Mount ----
    rebuild();

    return {
        destroy() {
            playing = false;
            if (rafId !== null) cancelAnimationFrame(rafId);
            container.removeChild(widget);
        },
    };
}
