/**
 * controls.js — Shared UI control components for the widget system.
 *
 * Exports: createSlider, createPlaybackControls, createStats
 */

// ---------------------------------------------------------------------------
// createSlider
// ---------------------------------------------------------------------------

/**
 * Create a labelled range slider.
 *
 * @param {object} opts
 * @param {string}   opts.label     Display label (shown as "label: value")
 * @param {number}   opts.min       Minimum value
 * @param {number}   opts.max       Maximum value
 * @param {number}   opts.value     Initial value
 * @param {number}   [opts.step=1]  Step size (default 1)
 * @param {function} opts.onChange  Called with Number when value changes
 * @returns {HTMLLabelElement}
 */
export function createSlider({ label, min, max, value, step = 1, onChange }) {
    const el = document.createElement('label');

    const span = document.createElement('span');
    span.textContent = `${label}: ${value}`;

    const input = document.createElement('input');
    input.type  = 'range';
    input.min   = min;
    input.max   = max;
    input.value = value;
    input.step  = step;

    input.addEventListener('input', () => {
        const num = Number(input.value);
        span.textContent = `${label}: ${num}`;
        onChange(num);
    });

    el.appendChild(span);
    el.appendChild(input);
    return el;
}

// ---------------------------------------------------------------------------
// createPlaybackControls
// ---------------------------------------------------------------------------

/**
 * Create Step / Play-Pause / Reset playback controls.
 *
 * @param {object}   opts
 * @param {function} opts.onStep      Called when Step is clicked
 * @param {function} opts.onPlayPause Called with boolean isPlaying on toggle
 * @param {function} opts.onReset     Called when Reset is clicked
 * @returns {{ element: HTMLElement, setPlaying: function(boolean): void }}
 */
export function createPlaybackControls({ onStep, onPlayPause, onReset }) {
    let isPlaying = false;

    const el = document.createElement('div');
    el.className = 'widget-controls';

    const stepBtn = document.createElement('button');
    stepBtn.textContent = 'Step';
    stepBtn.addEventListener('click', () => {
        onStep();
    });

    const playPauseBtn = document.createElement('button');
    playPauseBtn.textContent = '▶ Play';
    playPauseBtn.addEventListener('click', () => {
        isPlaying = !isPlaying;
        playPauseBtn.textContent = isPlaying ? '⏸ Pause' : '▶ Play';
        onPlayPause(isPlaying);
    });

    const resetBtn = document.createElement('button');
    resetBtn.textContent = 'Reset';
    resetBtn.addEventListener('click', () => {
        isPlaying = false;
        playPauseBtn.textContent = '▶ Play';
        onReset();
    });

    el.appendChild(stepBtn);
    el.appendChild(playPauseBtn);
    el.appendChild(resetBtn);

    /**
     * Externally set the play/pause state (e.g. when playback ends naturally).
     * @param {boolean} val
     */
    function setPlaying(val) {
        isPlaying = Boolean(val);
        playPauseBtn.textContent = isPlaying ? '⏸ Pause' : '▶ Play';
    }

    return { element: el, setPlaying };
}

// ---------------------------------------------------------------------------
// createStats
// ---------------------------------------------------------------------------

/**
 * Create a stats display div.
 *
 * @returns {{ element: HTMLElement, update: function(string): void }}
 */
export function createStats() {
    const el = document.createElement('div');
    el.className = 'widget-stats';

    function update(text) {
        el.textContent = text;
    }

    return { element: el, update };
}
