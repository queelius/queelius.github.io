/**
 * features.js — Hand-crafted feature extractors for linear function approximation.
 *
 * Each feature maps the current grid state to a scalar roughly in [0, 1].
 * The feature vector used for learning is a subset of these plus a bias term.
 *
 * Exports: FEATURES, extractFeatures, ALL_FEATURE_KEYS, DEFAULT_FEATURE_KEYS
 */

// ---------------------------------------------------------------------------
// Feature definitions
// ---------------------------------------------------------------------------

/**
 * Each entry:
 *   name        — display label for UI
 *   key         — unique identifier string
 *   description — tooltip / documentation text
 *   extract(grid) → number  (roughly normalized 0-1)
 */
export const FEATURES = [
    {
        name: 'Nearest Item Distance',
        key: 'nearest_item_dist',
        description: 'Manhattan distance to closest uncollected item, normalized by max grid distance.',
        extract(grid) {
            const maxDist = (grid.width - 1) + (grid.height - 1);
            if (maxDist === 0) return 0;

            let minDist = Infinity;
            for (let i = 0; i < grid.items.length; i++) {
                if (!(grid.bitmask & (1 << i))) continue; // already collected
                const item = grid.items[i];
                const d = Math.abs(grid.x - item.x) + Math.abs(grid.y - item.y);
                if (d < minDist) minDist = d;
            }

            if (minDist === Infinity) return 0; // no items remaining
            return minDist / maxDist;
        },
    },
    {
        name: 'Exit Distance',
        key: 'exit_dist',
        description: 'Manhattan distance to the exit, normalized by max grid distance.',
        extract(grid) {
            const maxDist = (grid.width - 1) + (grid.height - 1);
            if (maxDist === 0) return 0;
            const d = Math.abs(grid.x - grid.exit.x) + Math.abs(grid.y - grid.exit.y);
            return d / maxDist;
        },
    },
    {
        name: 'Items Remaining',
        key: 'items_remaining',
        description: 'Fraction of items still uncollected (0 = all collected, 1 = none collected).',
        extract(grid) {
            const total = grid.items.length;
            if (total === 0) return 0;
            let remaining = 0;
            for (let i = 0; i < total; i++) {
                if (grid.bitmask & (1 << i)) remaining++;
            }
            return remaining / total;
        },
    },
    {
        name: 'Nearest Item dX',
        key: 'nearest_item_dx',
        description: 'Signed X direction to nearest uncollected item, mapped to [0, 1] (0.5 = same column).',
        extract(grid) {
            let nearestX = null;
            let minDist = Infinity;
            for (let i = 0; i < grid.items.length; i++) {
                if (!(grid.bitmask & (1 << i))) continue;
                const item = grid.items[i];
                const d = Math.abs(grid.x - item.x) + Math.abs(grid.y - item.y);
                if (d < minDist) {
                    minDist = d;
                    nearestX = item.x;
                }
            }
            if (nearestX === null) return 0.5; // no items: neutral
            const dx = nearestX - grid.x;
            const maxDx = grid.width - 1;
            if (maxDx === 0) return 0.5;
            // Map signed dx from [-maxDx, maxDx] to [0, 1]
            return (dx + maxDx) / (2 * maxDx);
        },
    },
    {
        name: 'Nearest Item dY',
        key: 'nearest_item_dy',
        description: 'Signed Y direction to nearest uncollected item, mapped to [0, 1] (0.5 = same row).',
        extract(grid) {
            let nearestY = null;
            let minDist = Infinity;
            for (let i = 0; i < grid.items.length; i++) {
                if (!(grid.bitmask & (1 << i))) continue;
                const item = grid.items[i];
                const d = Math.abs(grid.x - item.x) + Math.abs(grid.y - item.y);
                if (d < minDist) {
                    minDist = d;
                    nearestY = item.y;
                }
            }
            if (nearestY === null) return 0.5; // no items: neutral
            const dy = nearestY - grid.y;
            const maxDy = grid.height - 1;
            if (maxDy === 0) return 0.5;
            return (dy + maxDy) / (2 * maxDy);
        },
    },
    {
        name: 'Exit dX',
        key: 'exit_dx',
        description: 'Signed X direction to the exit, mapped to [0, 1] (0.5 = same column as exit).',
        extract(grid) {
            const dx = grid.exit.x - grid.x;
            const maxDx = grid.width - 1;
            if (maxDx === 0) return 0.5;
            return (dx + maxDx) / (2 * maxDx);
        },
    },
    {
        name: 'Exit dY',
        key: 'exit_dy',
        description: 'Signed Y direction to the exit, mapped to [0, 1] (0.5 = same row as exit).',
        extract(grid) {
            const dy = grid.exit.y - grid.y;
            const maxDy = grid.height - 1;
            if (maxDy === 0) return 0.5;
            return (dy + maxDy) / (2 * maxDy);
        },
    },
    {
        name: 'Wall Density',
        key: 'wall_density',
        description: 'Fraction of wall cells in the 3x3 neighborhood centered on the agent.',
        extract(grid) {
            let wallCount = 0;
            let total = 0;
            for (let dy = -1; dy <= 1; dy++) {
                for (let dx = -1; dx <= 1; dx++) {
                    const nx = grid.x + dx;
                    const ny = grid.y + dy;
                    total++;
                    if (nx < 0 || nx >= grid.width || ny < 0 || ny >= grid.height) {
                        wallCount++; // out-of-bounds counts as wall
                    } else if (grid.isWall(nx, ny)) {
                        wallCount++;
                    }
                }
            }
            return wallCount / total;
        },
    },
    {
        name: 'Position X',
        key: 'pos_x',
        description: 'Normalized X position of the agent (0 = left edge, 1 = right edge).',
        extract(grid) {
            const maxX = grid.width - 1;
            if (maxX === 0) return 0;
            return grid.x / maxX;
        },
    },
    {
        name: 'Position Y',
        key: 'pos_y',
        description: 'Normalized Y position of the agent (0 = top edge, 1 = bottom edge).',
        extract(grid) {
            const maxY = grid.height - 1;
            if (maxY === 0) return 0;
            return grid.y / maxY;
        },
    },
];

// ---------------------------------------------------------------------------
// Key collections
// ---------------------------------------------------------------------------

/** All feature keys in declaration order. */
export const ALL_FEATURE_KEYS = FEATURES.map(f => f.key);

/**
 * Sensible default set: directional and count features, excluding absolute
 * position and wall density (which tend to be less informative on their own).
 */
export const DEFAULT_FEATURE_KEYS = [
    'nearest_item_dist',
    'exit_dist',
    'items_remaining',
    'nearest_item_dx',
    'nearest_item_dy',
    'exit_dx',
    'exit_dy',
];

// ---------------------------------------------------------------------------
// Feature lookup by key (built once)
// ---------------------------------------------------------------------------

const _featureByKey = new Map(FEATURES.map(f => [f.key, f]));

// ---------------------------------------------------------------------------
// extractFeatures
// ---------------------------------------------------------------------------

/**
 * Extract a numeric feature vector for the current grid state.
 *
 * @param {Grid}     grid        The environment (reads internal state directly)
 * @param {string[]} activeKeys  Ordered list of feature keys to include
 * @returns {number[]}  Feature vector with a bias term (1.0) appended at the end
 */
export function extractFeatures(grid, activeKeys) {
    const vec = new Array(activeKeys.length + 1);
    for (let i = 0; i < activeKeys.length; i++) {
        const feature = _featureByKey.get(activeKeys[i]);
        vec[i] = feature ? feature.extract(grid) : 0;
    }
    vec[activeKeys.length] = 1.0; // bias term
    return vec;
}
