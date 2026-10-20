/**
 * render.js — Canvas-based renderer for the collection gridworld.
 *
 * Exports: GridRenderer
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const CELL_SIZE = 32;
const TRAIL_MAX = 20;

// ---------------------------------------------------------------------------
// Color palettes
// ---------------------------------------------------------------------------

const THEMES = {
    dark: {
        wall:        '#1a1a2e',
        wallBorder:  '#0d0d1a',
        floor:       '#2d2d44',
        floorBorder: '#3a3a55',
        gridLine:    'rgba(255,255,255,0.06)',
        agent:       '#4a9eff',
        agentStroke: '#1a6ecc',
        exit:        '#44dd88',
        item:        '#ffd700',
        trail:       [80, 160, 255],   // RGB for trail dots
    },
    light: {
        wall:        '#8896a8',
        wallBorder:  '#6a7a8e',
        floor:       '#f0f4f8',
        floorBorder: '#dde4ec',
        gridLine:    'rgba(0,0,0,0.08)',
        agent:       '#1a6ecc',
        agentStroke: '#0d4fa0',
        exit:        '#1a9955',
        item:        '#cc8800',
        trail:       [26, 110, 204],   // RGB for trail dots
    },
};

// ---------------------------------------------------------------------------
// GridRenderer
// ---------------------------------------------------------------------------

export class GridRenderer {
    /**
     * @param {HTMLCanvasElement} canvas
     * @param {import('./grid.js').Grid} grid
     */
    constructor(canvas, grid) {
        this._canvas = canvas;
        this._ctx    = canvas.getContext('2d');
        this._grid   = grid;

        /** @type {Map<string, number>|null} stateKey → normalized [0,1] value */
        this._valueMap = null;

        /** @type {Map<string, number>|null} "x,y" → max value across all bitmasks (secondary index) */
        this._cellMaxValue = null;

        /** @type {Array<{x:number, y:number}>} recent agent positions, newest last */
        this._trail = [];

        this._sizeCanvas(grid);
    }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /**
     * Accept a Map<stateKey, normalizedValue> for value heatmap overlay.
     * normalizedValue should be in [0, 1] where 0=low (red) and 1=high (green).
     * Pass null to clear.
     *
     * Also builds a secondary index keyed by "x,y" → max value across all
     * bitmasks, so the renderer fallback lookup is O(1) instead of O(n).
     *
     * @param {Map<string,number>|null} map
     */
    setValueMap(map) {
        this._valueMap = map;
        // Rebuild the secondary "x,y" → maxValue index.
        if (map === null) {
            this._cellMaxValue = null;
        } else {
            this._cellMaxValue = new Map();
            for (const [key, val] of map) {
                const lastComma = key.lastIndexOf(',');
                const cellKey   = key.slice(0, lastComma); // "x,y"
                const cur       = this._cellMaxValue.get(cellKey);
                if (cur === undefined || val > cur) {
                    this._cellMaxValue.set(cellKey, val);
                }
            }
        }
    }

    /**
     * Track a recent agent position for the fading trail.
     * @param {number} x
     * @param {number} y
     */
    addTrailPoint(x, y) {
        this._trail.push({ x, y });
        if (this._trail.length > TRAIL_MAX) {
            this._trail.shift();
        }
    }

    /** Reset the trail. */
    clearTrail() {
        this._trail = [];
    }

    /**
     * Update for a new grid (different dimensions).
     * @param {import('./grid.js').Grid} grid
     */
    resize(grid) {
        this._grid = grid;
        this._sizeCanvas(grid);
    }

    /** Draw the full grid state to the canvas. */
    render() {
        const ctx    = this._ctx;
        const grid   = this._grid;
        const theme  = this._theme();
        const W      = grid.width;
        const H      = grid.height;
        const CS     = CELL_SIZE;

        // Clear
        ctx.clearRect(0, 0, this._canvas.width, this._canvas.height);

        // ------------------------------------------------------------------
        // Layer 1: Floor / wall cells
        // ------------------------------------------------------------------
        for (let y = 0; y < H; y++) {
            for (let x = 0; x < W; x++) {
                const px = x * CS;
                const py = y * CS;
                if (grid.isWall(x, y)) {
                    ctx.fillStyle = theme.wall;
                    ctx.fillRect(px, py, CS, CS);
                } else {
                    ctx.fillStyle = theme.floor;
                    ctx.fillRect(px, py, CS, CS);
                }
            }
        }

        // ------------------------------------------------------------------
        // Layer 2: Value function overlay (floor cells only)
        // ------------------------------------------------------------------
        if (this._valueMap !== null) {
            for (let y = 0; y < H; y++) {
                for (let x = 0; x < W; x++) {
                    if (grid.isWall(x, y)) continue;

                    // Build all bitmask values to find the max value for this cell.
                    // The state key encodes (x, y, bitmask); we sample the current
                    // bitmask as the primary lookup, falling back to a scan if absent.
                    const state    = grid.getState();
                    const key      = `${x},${y},${state.bitmask}`;
                    let   value    = this._valueMap.get(key);

                    if (value === undefined) {
                        // Fall back: find best value across all bitmask variants.
                        value = this._bestValueForCell(x, y);
                    }

                    if (value !== undefined) {
                        const v = Math.max(0, Math.min(1, value));
                        // Red (low) → Green (high), ~35% opacity
                        const r = Math.round(255 * (1 - v));
                        const g = Math.round(255 * v);
                        ctx.fillStyle = `rgba(${r},${g},0,0.35)`;
                        ctx.fillRect(x * CS, y * CS, CS, CS);
                    }
                }
            }
        }

        // ------------------------------------------------------------------
        // Layer 3: Grid lines
        // ------------------------------------------------------------------
        ctx.strokeStyle = theme.gridLine;
        ctx.lineWidth   = 1;
        for (let x = 0; x <= W; x++) {
            ctx.beginPath();
            ctx.moveTo(x * CS + 0.5, 0);
            ctx.lineTo(x * CS + 0.5, H * CS);
            ctx.stroke();
        }
        for (let y = 0; y <= H; y++) {
            ctx.beginPath();
            ctx.moveTo(0,     y * CS + 0.5);
            ctx.lineTo(W * CS, y * CS + 0.5);
            ctx.stroke();
        }

        // ------------------------------------------------------------------
        // Layer 4 (part a): Agent trail — fading dots
        // ------------------------------------------------------------------
        const [tr, tg, tb] = theme.trail;
        const n = this._trail.length;
        for (let i = 0; i < n; i++) {
            const { x, y } = this._trail[i];
            // i=0 is oldest (most faded), i=n-1 is newest (brightest)
            const alpha  = 0.08 + 0.55 * (i / Math.max(n - 1, 1));
            const radius = 4 + 4 * (i / Math.max(n - 1, 1));
            ctx.beginPath();
            ctx.arc(
                x * CS + CS / 2,
                y * CS + CS / 2,
                radius,
                0, Math.PI * 2
            );
            ctx.fillStyle = `rgba(${tr},${tg},${tb},${alpha.toFixed(3)})`;
            ctx.fill();
        }

        // ------------------------------------------------------------------
        // Layer 5: Items — gold star at uncollected item positions
        // ------------------------------------------------------------------
        ctx.fillStyle = theme.item;
        ctx.font      = `${Math.round(CS * 0.55)}px serif`;
        ctx.textAlign    = 'center';
        ctx.textBaseline = 'middle';
        for (const { x, y } of grid.items) {
            if (grid.isItemPresent(x, y)) {
                ctx.fillText('★', x * CS + CS / 2, y * CS + CS / 2);
            }
        }

        // ------------------------------------------------------------------
        // Layer 6: Exit — green diamond character
        // ------------------------------------------------------------------
        const { x: ex, y: ey } = grid.exit;
        ctx.fillStyle = theme.exit;
        ctx.font      = `${Math.round(CS * 0.60)}px serif`;
        ctx.textAlign    = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('◆', ex * CS + CS / 2, ey * CS + CS / 2);

        // ------------------------------------------------------------------
        // Layer 7: Agent — blue filled circle
        // ------------------------------------------------------------------
        const { x: ax, y: ay } = grid.getState();
        const cx = ax * CS + CS / 2;
        const cy = ay * CS + CS / 2;
        const r  = Math.round(CS * 0.30);

        ctx.beginPath();
        ctx.arc(cx, cy, r, 0, Math.PI * 2);
        ctx.fillStyle   = theme.agent;
        ctx.fill();
        ctx.strokeStyle = theme.agentStroke;
        ctx.lineWidth   = 2;
        ctx.stroke();
    }

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /** Size the canvas to fit the grid. */
    _sizeCanvas(grid) {
        this._canvas.width  = grid.width  * CELL_SIZE;
        this._canvas.height = grid.height * CELL_SIZE;
    }

    /** Detect current color scheme preference. */
    _theme() {
        const prefersDark = typeof window !== 'undefined'
            && window.matchMedia
            && window.matchMedia('(prefers-color-scheme: dark)').matches;
        return prefersDark ? THEMES.dark : THEMES.light;
    }

    /**
     * Look up the best (max) value associated with cell (x, y) across all
     * bitmask variants.  Uses the precomputed secondary index built by
     * setValueMap() for O(1) lookup.  Returns undefined if none found.
     * @param {number} x
     * @param {number} y
     * @returns {number|undefined}
     */
    _bestValueForCell(x, y) {
        if (!this._cellMaxValue) return undefined;
        return this._cellMaxValue.get(`${x},${y}`);
    }
}
