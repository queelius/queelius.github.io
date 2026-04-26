/**
 * Reusable speech synthesis library.
 *
 * One module-level Speaker handles all current playback. Calling speak()
 * while playback is in progress cancels the previous job cleanly.
 *
 * Used by both the standalone /speak/ page and the {{< speak >}} shortcode.
 */

import * as tts from "https://cdn.jsdelivr.net/npm/@mintplex-labs/piper-tts-web@1.0.4/dist/piper-tts-web.js";

export const VOICES = [
  { id: "en_US-amy-low", label: "Amy (US, low) — ~22 MB, fastest" },
  { id: "en_US-ryan-low", label: "Ryan (US male, low) — ~22 MB" },
  { id: "en_GB-alan-low", label: "Alan (UK male, low) — ~22 MB" },
  { id: "en_US-amy-medium", label: "Amy (US, medium) — ~64 MB, better quality" },
  { id: "en_US-libritts_r-medium", label: "LibriTTS-R (US, medium) — ~64 MB, multi-speaker" },
  { id: "en_GB-jenny_dioco-medium", label: "Jenny (UK female, medium) — ~64 MB" },
  { id: "en_GB-northern_english_male-medium", label: "Northern English Male — ~64 MB" },
];

const downloaded = new Set();
let cancelToken = 0;
let activeQueue = null;

/**
 * Decide whether a parenthetical's content is an aside that can be safely
 * dropped from narration. Returns true for things like citations, page
 * refs, and brief asides; false for prose-load-bearing parentheticals
 * that the surrounding sentences may reference.
 */
function shouldStripParen(content) {
  const c = content.trim();
  if (!c) return true;
  // Year alone or with letter suffix: (2024), (2024a)
  if (/^\d{1,4}[a-z]?$/.test(c)) return true;
  // Reference markers: (see X), (cf. X), (p. 5), (Fig. 3), etc.
  if (/^(see|cf\.?|e\.g\.?|i\.e\.?|viz\.?|et\s+al\.?|ibid\.?|p\.|pp\.|chap\.?|chapter\s|fig\.?|figure\s|table\s|eq\.?|equation\s|sec\.?|section\s|n\.b\.?)\b/i.test(c)) return true;
  // Citation-shaped: contains a 4-digit year and is short
  const wordCount = c.split(/\s+/).filter(Boolean).length;
  if (/\b\d{4}[a-z]?\b/.test(c) && wordCount <= 6) return true;
  // Short asides: 4 or fewer words. Prose-load-bearing parens tend to
  // be longer. This is a heuristic; per-post override exists.
  if (wordCount <= 4) return true;
  return false;
}

/**
 * Process parenthetical asides for narration flow.
 *
 * Citations and short asides are dropped. Longer prose parentheticals
 * are kept but rebraced with commas, so the TTS reads them as natural
 * asides instead of pronouncing the surrounding sentence as if the
 * parenthetical content didn't exist.
 *
 * Doesn't touch square brackets, curly braces, or LaTeX delimiters
 * \(...\) (negative lookbehind on backslash). Content inside inline
 * <code> is already filtered upstream at the DOM level.
 */
export function stripParens(text) {
  let out = text.replace(/(?<!\\)\(([^)]*)\)/g, (match, content) =>
    shouldStripParen(content) ? "" : `, ${content.trim()},`,
  );
  // Cleanup: comma before terminal punctuation, doubled commas, spaces
  out = out.replace(/,\s*([.!?])/g, "$1");
  out = out.replace(/,\s*,/g, ",");
  out = out.replace(/\s+([,.;:!?])/g, "$1");
  out = out.replace(/\s{2,}/g, " ");
  return out.trim();
}

/**
 * Split text into chunks suitable for sentence-level streaming.
 * Splits on sentence boundaries first, then breaks oversized chunks at
 * clause boundaries, then by word as a last resort.
 */
export function chunkText(text, maxLen = 240) {
  // Sentence boundary = .!? optionally followed by a closing quote or
  // bracket, then whitespace or EOS. Without the closing-quote allowance,
  // text like `said "yes." Then` silently drops the first sentence
  // because the regex can't satisfy the `\s|$` lookahead after `.`.
  const sentences =
    text.match(/[^.!?]+[.!?]+["'”’\)\]]*(?:\s|$)|[^.!?]+$/g) ||
    [text];
  const out = [];
  for (const raw of sentences) {
    const s = raw.trim();
    if (!s) continue;
    if (s.length <= maxLen) {
      out.push(s);
      continue;
    }
    let buf = "";
    const subparts = s.split(/(?<=[,;:])\s+/);
    for (const sp of subparts) {
      if ((buf + " " + sp).trim().length > maxLen && buf) {
        out.push(buf.trim());
        buf = sp;
      } else {
        buf = buf ? buf + " " + sp : sp;
      }
    }
    if (buf.trim()) {
      if (buf.length > maxLen) {
        const words = buf.split(/\s+/);
        let wbuf = "";
        for (const w of words) {
          if ((wbuf + " " + w).trim().length > maxLen && wbuf) {
            out.push(wbuf.trim());
            wbuf = w;
          } else {
            wbuf = wbuf ? wbuf + " " + w : w;
          }
        }
        if (wbuf.trim()) out.push(wbuf.trim());
      } else {
        out.push(buf.trim());
      }
    }
  }
  return out;
}

/**
 * Sequential audio playback via HTMLAudioElement.
 *
 * Uses HTMLMediaElement's `preservesPitch` property so playback rate
 * changes are time-stretches, not pitch shifts (no chipmunk effect).
 * Modern browsers default to preservesPitch=true; we set it explicitly
 * with vendor prefixes for older Safari/Firefox.
 *
 * Trade-off vs Web Audio: the inter-chunk gap is browser-dependent
 * (typically 10-50ms) instead of sample-accurate zero, but for
 * sentence-boundary TTS this is imperceptible.
 */
function setPreservePitch(audio, val) {
  audio.preservesPitch = val;
  audio.mozPreservesPitch = val;
  audio.webkitPreservesPitch = val;
}

class AudioQueue {
  constructor(rate = 1.0) {
    this.rate = Math.max(0.25, Math.min(4.0, rate));
    this.audio = new Audio();
    setPreservePitch(this.audio, true);
    this.audio.playbackRate = this.rate;
    this.queue = []; // [{ url, onStart }]
    this.current = null; // { url, onStart }
    this.draining = null; // promise resolved when queue empties
    this.audio.addEventListener("ended", () => this._advance());
  }
  /**
   * Append a WAV blob. Optional onStart fires when this chunk's audio
   * actually begins playing.
   */
  enqueue(blob, opts = {}) {
    const url = URL.createObjectURL(blob);
    this.queue.push({ url, onStart: opts.onStart || null });
    if (!this.current) this._advance();
    return Promise.resolve();
  }
  _advance() {
    // Resolve drain if there's nothing left
    if (this.current) {
      URL.revokeObjectURL(this.current.url);
      this.current = null;
    }
    if (!this.queue.length) {
      if (this.draining) {
        this.draining.resolve();
        this.draining = null;
      }
      return;
    }
    this.current = this.queue.shift();
    this.audio.src = this.current.url;
    this.audio.playbackRate = this.rate;
    setPreservePitch(this.audio, true);
    const playPromise = this.audio.play();
    if (playPromise && typeof playPromise.then === "function") {
      playPromise
        .then(() => this.current && this.current.onStart && this.current.onStart())
        .catch((err) => console.warn("audio.play() rejected:", err));
    }
  }
  setRate(rate) {
    this.rate = Math.max(0.25, Math.min(4.0, rate));
    if (this.audio) this.audio.playbackRate = this.rate;
  }
  stop() {
    try { this.audio.pause(); } catch (_) { /* */ }
    if (this.current) URL.revokeObjectURL(this.current.url);
    for (const item of this.queue) URL.revokeObjectURL(item.url);
    this.queue = [];
    this.current = null;
    if (this.draining) {
      this.draining.resolve();
      this.draining = null;
    }
  }
  /** Resolves when the queue is empty AND nothing is currently playing. */
  awaitDrain() {
    if (!this.current && !this.queue.length) return Promise.resolve();
    if (this.draining) return this.draining.promise;
    let resolveFn;
    const promise = new Promise((r) => { resolveFn = r; });
    this.draining = { promise, resolve: resolveFn };
    return promise;
  }
}

async function ensureVoice(voiceId, onProgress) {
  if (downloaded.has(voiceId)) return;
  await tts.download(voiceId, (p) => {
    if (onProgress && p.total) onProgress(p.loaded, p.total);
  });
  downloaded.add(voiceId);
}

/**
 * Speak text or a sequence of blocks using the given voice. Streaming:
 * synthesis is pipelined with playback, so the listener hears the first
 * chunk as soon as it's ready while later chunks synthesize in the
 * background. Calling speak() again cancels the in-flight job.
 *
 * Input forms:
 *   speak("one big string", voiceId, callbacks)
 *   speak(["block 1 text", "block 2 text"], voiceId, callbacks)
 *
 * Callbacks (all optional):
 *   rate                       — playback rate (0.5 .. 2.0, default 1.0)
 *   onStatus(text, kind)       — status updates ("loading", "ready", "error")
 *   onProgress(loaded, total)  — voice-download progress (bytes)
 *   onChunkStart(idx, total)   — first chunk has begun playing
 *   onBlockStart(blockIdx)     — fires at wall-clock moment block N starts
 *   onDone(chunkCount)         — playback fully finished
 *   onError(err)               — synthesis or playback error
 */
export async function speak(input, voiceId, callbacks = {}) {
  stop();
  cancelToken += 1;
  const myToken = cancelToken;
  const cb = callbacks;

  // Normalize input to an array of block strings.
  let blocks = (typeof input === "string" ? [input] : input)
    .map((s) => (typeof s === "string" ? s : s.text || ""))
    .map((s) => s.trim())
    .filter(Boolean);
  // Default behaviour: skip parenthetical asides (citations, brief
  // clarifications). Opt out by passing keepParens: true.
  if (!callbacks.keepParens) {
    blocks = blocks.map(stripParens).filter(Boolean);
  }
  if (!blocks.length) return;

  // Chunk each block, track which chunks belong to which block so we
  // can emit onBlockStart at the right wall-clock time.
  const chunks = [];
  const chunkBlockIdx = [];
  for (let bi = 0; bi < blocks.length; bi++) {
    for (const c of chunkText(blocks[bi])) {
      chunks.push(c);
      chunkBlockIdx.push(bi);
    }
  }
  if (!chunks.length) return;

  try {
    cb.onStatus?.(`Loading voice ${voiceId}...`, "loading");
    await ensureVoice(voiceId, cb.onProgress);
    if (myToken !== cancelToken) return;

    activeQueue = new AudioQueue(callbacks.rate);
    cb.onStatus?.(
      `Synthesizing ${chunks.length} chunk${chunks.length !== 1 ? "s" : ""}...`,
      "loading",
    );

    let lastBlockSeen = -1;
    for (let i = 0; i < chunks.length; i++) {
      if (myToken !== cancelToken) return;
      const wav = await tts.predict({ text: chunks[i], voiceId });
      if (myToken !== cancelToken) return;
      const blockIdx = chunkBlockIdx[i];
      const isFirstOfBlock = blockIdx !== lastBlockSeen;
      lastBlockSeen = blockIdx;
      const onStart = () => {
        if (myToken !== cancelToken) return;
        if (i === 0) cb.onChunkStart?.(0, chunks.length);
        if (isFirstOfBlock) cb.onBlockStart?.(blockIdx);
      };
      activeQueue.enqueue(wav, { onStart });
    }
    await activeQueue.awaitDrain();
    if (myToken === cancelToken) {
      cb.onStatus?.("Done.", "ready");
      cb.onDone?.(chunks.length);
    }
  } catch (err) {
    console.error(err);
    if (myToken === cancelToken) {
      cb.onStatus?.("Error: " + err.message, "error");
      cb.onError?.(err);
    }
  }
}

export function stop() {
  cancelToken += 1;
  if (activeQueue) {
    activeQueue.stop();
    activeQueue = null;
  }
}

/** Live-update playback rate of the currently-active queue (no restart). */
export function setActiveRate(rate) {
  if (activeQueue) activeQueue.setRate(rate);
}

export function isSpeaking() {
  return activeQueue !== null && activeQueue.activeSources.size > 0;
}
