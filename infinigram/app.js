/**
 * /infinigram/ page: token-level continuation explorer.
 *
 * Tokenizer: SmolLM2 (loaded via Transformers.js from local files).
 * Index: token-level suffix array over the blog corpus, built offline
 * with the same tokenizer.
 *
 * Tokenization happens in two places that must match exactly:
 *   - Build time (Python, transformers.AutoTokenizer)
 *   - Query time (browser, @huggingface/transformers AutoTokenizer)
 * Both load the same tokenizer.json, so token IDs align.
 */

import {
  load,
  count,
  continuations,
  longestSuffixMatch,
  sample,
  contextAround,
  positions,
  corpusSize,
  vocabSize,
} from "/infinigram/lib.js";

// Pure-JS tokenizer (~80 KB, no ONNX dependency). Constructs directly
// from a parsed tokenizer.json + tokenizer_config.json.
import { Tokenizer } from "https://cdn.jsdelivr.net/npm/@huggingface/tokenizers@0.1.3/dist/tokenizers.mjs";

const els = {
  status: document.getElementById("ig-status"),
  loadBtn: document.getElementById("ig-load"),
  ui: document.getElementById("ig-ui"),
  input: document.getElementById("ig-input"),
  match: document.getElementById("ig-match"),
  conts: document.getElementById("ig-conts"),
  examples: document.getElementById("ig-examples"),
  tempInput: document.getElementById("ig-temperature"),
  tempValue: document.getElementById("ig-temperature-value"),
  generate: document.getElementById("ig-generate"),
  stop: document.getElementById("ig-stop"),
  output: document.getElementById("ig-output"),
};

let tokenizer = null;
let generating = false;

function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === "class") node.className = v;
    else if (k === "text") node.textContent = v;
    else node.setAttribute(k, v);
  }
  for (const c of children) {
    if (c == null) continue;
    node.appendChild(typeof c === "string" ? document.createTextNode(c) : c);
  }
  return node;
}

function setStatus(text, kind = "info") {
  els.status.textContent = text;
  els.status.dataset.kind = kind;
}

function clear(node) {
  while (node.firstChild) node.removeChild(node.firstChild);
}

function visibleNewline(s) {
  return s.replace(/\n/g, "↵ ");
}

/**
 * Encode a string to a Uint32Array of SmolLM2 token IDs, without special
 * tokens (we want pure prose distributions, not BOS/EOS).
 */
function encodeText(text) {
  if (!tokenizer) throw new Error("tokenizer not loaded");
  const enc = tokenizer.encode(text, { add_special_tokens: false });
  return new Uint32Array(enc.ids);
}

/**
 * Decode a Uint32Array (or Array) of token IDs to a string.
 */
function decodeTokens(tokens) {
  if (!tokenizer) throw new Error("tokenizer not loaded");
  const arr = tokens instanceof Uint32Array ? Array.from(tokens) : tokens;
  return tokenizer.decode(arr, { skip_special_tokens: false });
}

function decodeSingleToken(id) {
  return tokenizer.decode([id], { skip_special_tokens: false });
}

function renderMatch(text) {
  clear(els.match);
  clear(els.conts);
  clear(els.examples);

  if (!text.length || !tokenizer) {
    els.match.appendChild(document.createTextNode("Type something to query."));
    return;
  }

  const tokens = encodeText(text);
  if (!tokens.length) {
    els.match.appendChild(document.createTextNode("Empty after tokenization."));
    return;
  }

  const m = longestSuffixMatch(tokens, 1);
  if (m.suffixLen === 0) {
    els.match.appendChild(document.createTextNode(
      "No token suffix of your input occurs in the corpus.",
    ));
    return;
  }

  // --- Match line ---
  const truncated = tokens.length - m.suffixLen;
  const matchedText = decodeTokens(m.matchedTokens);
  const lead = el("span", { class: "ig-match-lead" },
    `Longest match: ${m.suffixLen} token${m.suffixLen !== 1 ? "s" : ""} ` +
    `(${tokens.length} input → ${m.suffixLen} matched), ` +
    `${m.count.toLocaleString()} occurrence${m.count !== 1 ? "s" : ""}: `);
  els.match.appendChild(lead);
  if (truncated > 0) {
    const truncTokens = tokens.subarray(0, truncated);
    const truncText = decodeTokens(truncTokens);
    els.match.appendChild(
      el("span", { class: "ig-trunc", title: "tokens not in corpus" },
        "…" + visibleNewline(truncText)),
    );
  }
  els.match.appendChild(
    el("span", { class: "ig-match-text" }, visibleNewline(matchedText)),
  );

  // --- Continuations table ---
  const conts = continuations(m.matchedTokens, 12);
  if (!conts.length) {
    els.conts.appendChild(el("div", { class: "ig-empty" },
      "No continuations: this match runs to end of corpus only."));
  } else {
    const table = el("table", { class: "ig-table" },
      el("thead", {},
        el("tr", {},
          el("th", { text: "next token" }),
          el("th", { text: "id" }),
          el("th", { text: "count" }),
          el("th", { text: "p" }),
        )),
    );
    const tbody = el("tbody");
    for (const c of conts) {
      const tokenStr = decodeSingleToken(c.token);
      const visibleStr = visibleNewline(tokenStr) || "(empty)";
      const tokenCode = el("code", { text: visibleStr });
      const bar = el("div", { class: "ig-bar" });
      bar.style.width = (c.prob * 100).toFixed(1) + "%";
      tbody.appendChild(
        el("tr", {},
          el("td", { class: "ig-cont-char" }, tokenCode),
          el("td", { class: "ig-cont-byte", text: c.token.toString() }),
          el("td", { class: "ig-cont-count", text: c.count.toLocaleString() }),
          el("td", { class: "ig-cont-prob" },
            (c.prob * 100).toFixed(1) + "% ", bar),
        ),
      );
    }
    table.appendChild(tbody);
    els.conts.appendChild(table);
  }

  // --- Example contexts ---
  const exPositions = positions(m.matchedTokens, 5);
  for (const pos of exPositions) {
    const beforeTokens = contextAround(pos, 15, 0);
    const matchedTokens = contextAround(pos, 0, m.suffixLen);
    const afterTokens = contextAround(pos + m.suffixLen, 0, 15);
    const before = decodeTokens(beforeTokens);
    const matched = decodeTokens(matchedTokens);
    const after = decodeTokens(afterTokens);
    const li = el("li", {},
      el("code", {},
        visibleNewline(before),
        el("mark", { text: visibleNewline(matched) }),
        visibleNewline(after),
      ),
    );
    els.examples.appendChild(li);
  }
}

async function loadAll() {
  els.loadBtn.disabled = true;
  setStatus("Loading suffix array...", "loading");
  try {
    await load({
      onProgress: (p) => setStatus(p.phase + "...", "loading"),
    });
    setStatus("Loading tokenizer...", "loading");
    const [tokJson, tokCfg] = await Promise.all([
      fetch("/infinigram/tokenizer/tokenizer.json").then((r) => r.json()),
      fetch("/infinigram/tokenizer/tokenizer_config.json").then((r) => r.json()),
    ]);
    tokenizer = new Tokenizer(tokJson, tokCfg);
    const size = corpusSize();
    const vocab = vocabSize();
    setStatus(
      `Ready: ${size.toLocaleString()} tokens, vocab ${vocab.toLocaleString()}.`,
      "ready",
    );
    els.ui.hidden = false;
    els.loadBtn.hidden = true;
    els.input.focus();
  } catch (err) {
    console.error(err);
    setStatus("Error: " + err.message, "error");
    els.loadBtn.disabled = false;
  }
}

function debounce(fn, ms) {
  let t = null;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}

const onInput = debounce(() => renderMatch(els.input.value), 80);

async function generateLoop() {
  if (generating || !tokenizer) return;
  generating = true;
  els.generate.disabled = true;
  els.stop.hidden = false;

  const temperature = parseFloat(els.tempInput.value);
  const startText = els.input.value;
  const prefix = encodeText(startText);
  let context = new Uint32Array(prefix);
  els.output.textContent = startText;

  const MAX_TOKENS = 80;
  for (let step = 0; step < MAX_TOKENS && generating; step++) {
    const next = sample(context, { temperature });
    if (next === null) break;
    const newCtx = new Uint32Array(context.length + 1);
    newCtx.set(context);
    newCtx[context.length] = next;
    context = newCtx;

    const generatedTokens = context.subarray(prefix.length);
    els.output.textContent = startText + decodeTokens(generatedTokens);

    if (step % 4 === 0) await new Promise((r) => setTimeout(r, 0));
  }

  generating = false;
  els.generate.disabled = false;
  els.stop.hidden = true;
}

function stopGenerate() {
  generating = false;
}

els.loadBtn.addEventListener("click", loadAll);
els.input.addEventListener("input", onInput);
els.tempInput.addEventListener("input", () => {
  els.tempValue.textContent = parseFloat(els.tempInput.value).toFixed(2);
});
els.generate.addEventListener("click", generateLoop);
els.stop.addEventListener("click", stopGenerate);

setStatus("Click Load to fetch the corpus, suffix array, and tokenizer (~6 MB).", "idle");
