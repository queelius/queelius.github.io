/**
 * Token-level browser-side query layer for an infinigram suffix array
 * built over the SmolLM2 tokenizer's vocabulary.
 *
 * The corpus is a Uint32Array of token IDs (encoded with the same
 * tokenizer the LLM uses). The suffix array is also Uint32Array; each
 * entry is a token-position index into the corpus.
 *
 * Operations:
 *   findRange(pattern: Uint32Array)
 *   count(pattern: Uint32Array)
 *   continuations(pattern, k)  → top-k next token ids with counts
 *   longestSuffixMatch(context, minCount)
 *   sample(context, opts)      → next token id by sampling
 *   positions(pattern, limit)  → corpus positions for example contexts
 *
 * All operations are O(m log n) or better. The same lib will eventually
 * mix into SmolLM2's logits for register-aware decoding.
 */

let corpus = null; // Uint32Array of token IDs
let sa = null; // Uint32Array of suffix-array indices
let meta = null;
let loaded = false;
let loading = null;
let loadedBase = null;

const DEFAULT_BASE = "/infinigram";

/**
 * Load corpus + suffix-array files. By default fetches from
 * /infinigram/, but pass `base` to load a different corpus (e.g.,
 * "/alex" for the chat-log infinigram).
 *
 * Loading a different base after a previous load forces a reload.
 */
export async function load(opts = {}) {
  const base = (opts.base ?? DEFAULT_BASE).replace(/\/$/, "");
  if (loaded && loadedBase === base) return;
  if (loading) return loading;
  if (loaded && loadedBase !== base) {
    // switching corpus: drop existing buffers
    corpus = null;
    sa = null;
    meta = null;
    loaded = false;
    loadedBase = null;
  }
  const onProgress = opts.onProgress;
  loading = (async () => {
    onProgress?.({ phase: "fetching meta" });
    const metaResp = await fetch(`${base}/meta.tokens.json`);
    if (!metaResp.ok) throw new Error("meta.tokens.json: " + metaResp.status);
    meta = await metaResp.json();

    onProgress?.({ phase: "fetching corpus" });
    const corpusResp = await fetch(`${base}/corpus.tokens.bin`);
    if (!corpusResp.ok) throw new Error("corpus.tokens.bin: " + corpusResp.status);
    corpus = new Uint32Array(await corpusResp.arrayBuffer());

    onProgress?.({ phase: "fetching suffix array" });
    const saResp = await fetch(`${base}/sa.tokens.bin`);
    if (!saResp.ok) throw new Error("sa.tokens.bin: " + saResp.status);
    sa = new Uint32Array(await saResp.arrayBuffer());

    if (sa.length !== corpus.length) {
      throw new Error(`SA length ${sa.length} != corpus length ${corpus.length}`);
    }
    loaded = true;
    loadedBase = base;
    onProgress?.({
      phase: "ready",
      n_tokens: corpus.length,
      vocab_size: meta.vocab_size,
      base,
    });
  })();
  try {
    await loading;
  } finally {
    loading = null;
  }
}

/**
 * Compare the suffix at corpus[saIdx ..] (in token units) to a pattern,
 * token-by-token for up to pattern.length tokens.
 *
 *  < 0 if suffix < pattern lexicographically over token IDs
 *  > 0 if suffix > pattern
 * == 0 if suffix starts with pattern
 */
function cmpSuffixToPattern(saIdx, pattern) {
  const start = sa[saIdx];
  const remain = corpus.length - start;
  const limit = Math.min(pattern.length, remain);
  for (let i = 0; i < limit; i++) {
    const a = corpus[start + i];
    const b = pattern[i];
    if (a !== b) return a - b;
  }
  return remain < pattern.length ? -1 : 0;
}

/**
 * Find the SA range [lo, hi) of suffixes that begin with `pattern`.
 * Empty range (lo == hi) means no matches.
 */
export function findRange(pattern) {
  if (!loaded) throw new Error("call load() first");
  if (!pattern.length) return [0, sa.length];
  let lo = 0;
  let hi = sa.length;
  while (lo < hi) {
    const mid = (lo + hi) >>> 1;
    if (cmpSuffixToPattern(mid, pattern) < 0) lo = mid + 1;
    else hi = mid;
  }
  const start = lo;
  hi = sa.length;
  let upperLo = start;
  while (upperLo < hi) {
    const mid = (upperLo + hi) >>> 1;
    if (cmpSuffixToPattern(mid, pattern) <= 0) upperLo = mid + 1;
    else hi = mid;
  }
  return [start, upperLo];
}

export function count(pattern) {
  const [lo, hi] = findRange(pattern);
  return hi - lo;
}

/**
 * Top-k token ids that follow `pattern` in the corpus.
 */
export function continuations(pattern, k = 10) {
  const [lo, hi] = findRange(pattern);
  if (lo === hi) return [];
  const counts = new Map();
  const offset = pattern.length;
  for (let i = lo; i < hi; i++) {
    const pos = sa[i] + offset;
    if (pos >= corpus.length) continue;
    const t = corpus[pos];
    counts.set(t, (counts.get(t) || 0) + 1);
  }
  const total = hi - lo;
  const out = [];
  for (const [token, c] of counts) out.push({ token, count: c, prob: c / total });
  out.sort((a, b) => b.count - a.count);
  return out.slice(0, k);
}

/**
 * Longest suffix of `context` (in tokens) that occurs at least
 * `minCount` times in the corpus.
 */
export function longestSuffixMatch(contextTokens, minCount = 1) {
  for (let len = contextTokens.length; len > 0; len--) {
    const suffix = contextTokens.subarray(contextTokens.length - len);
    const c = count(suffix);
    if (c >= minCount) {
      return { matchedTokens: suffix, count: c, suffixLen: len };
    }
  }
  return { matchedTokens: new Uint32Array(0), count: 0, suffixLen: 0 };
}

/**
 * Sample a next token id given a context. Falls back to shorter
 * suffixes (n-gram backoff) if the longest context has no continuations.
 *
 * opts.minCount     — minimum match count to consider (default 1)
 * opts.temperature  — 0.0 = argmax, 1.0 = empirical probabilities
 */
export function sample(contextTokens, opts = {}) {
  const minCount = opts.minCount ?? 1;
  const temperature = opts.temperature ?? 1.0;
  const m = longestSuffixMatch(contextTokens, minCount);
  if (m.suffixLen === 0) return null;
  const conts = continuations(m.matchedTokens, 256);
  if (!conts.length) return null;
  if (temperature <= 0.001) return conts[0].token;
  let total = 0;
  const weights = [];
  for (const c of conts) {
    const w = Math.pow(c.prob, 1 / temperature);
    weights.push(w);
    total += w;
  }
  let r = Math.random() * total;
  for (let i = 0; i < conts.length; i++) {
    r -= weights[i];
    if (r <= 0) return conts[i].token;
  }
  return conts[conts.length - 1].token;
}

/**
 * Window of token ids around a given corpus position, for showing
 * context. Returns a Uint32Array slice that the caller can decode.
 */
export function contextAround(pos, before = 20, after = 20) {
  const lo = Math.max(0, pos - before);
  const hi = Math.min(corpus.length, pos + after);
  return corpus.subarray(lo, hi);
}

/**
 * Up to `limit` corpus token-positions where `pattern` occurs.
 */
export function positions(pattern, limit = 10) {
  const [lo, hi] = findRange(pattern);
  if (lo === hi) return [];
  const total = hi - lo;
  const n = Math.min(limit, total);
  const out = new Array(n);
  for (let i = 0; i < n; i++) {
    const idx = lo + Math.floor((total / n) * i);
    out[i] = sa[idx];
  }
  return out;
}

export function corpusSize() {
  return loaded ? corpus.length : 0;
}

export function vocabSize() {
  return meta ? meta.vocab_size : 0;
}

export function tokenizerId() {
  return meta ? meta.tokenizer_id : null;
}
