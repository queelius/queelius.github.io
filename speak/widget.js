/**
 * Speak widget: auto-bind any element with class="speak-button".
 *
 * Adjacent to the button we may also render a settings panel with:
 *   - voice picker
 *   - playback rate slider
 *
 * As each block plays, the corresponding DOM element gets a
 * `speak-current` class so a CSS rule can highlight it.
 */

import { speak, stop, setActiveRate, VOICES } from "/speak/lib.js";

// LibriTTS-R is multi-speaker, ~64 MB, sounds noticeably more natural
// than amy-low. Pays for itself with one listen-through.
const DEFAULT_VOICE = "en_US-libritts_r-medium";
const DEFAULT_RATE = 1.25;

// ---------------------------------------------------------------------------
// Block-aware text extraction with DOM tracking
// ---------------------------------------------------------------------------

const SPEAKABLE_SELECTOR = "p, li, blockquote, dt, dd, h2, h3, h4, h5, h6";
const SKIP_SELECTOR = "pre, code, .speak-widget, .speak-button, .speak-controls, figure, .footnote-backref, sup.footnote-ref";

function getArticleRoot() {
  return (
    document.querySelector(".content[itemprop='articleBody']") ||
    document.querySelector(".content") ||
    document.querySelector("article") ||
    document.querySelector("main")
  );
}

function extractBlocks(root) {
  const blocks = [];
  const elements = root.querySelectorAll(SPEAKABLE_SELECTOR);
  for (const el of elements) {
    if (el.closest(SKIP_SELECTOR)) continue;
    let parentInList = false;
    let p = el.parentElement;
    while (p && p !== root) {
      if (p.matches && p.matches(SPEAKABLE_SELECTOR) && !p.closest(SKIP_SELECTOR)) {
        parentInList = true;
        break;
      }
      p = p.parentElement;
    }
    if (parentInList) continue;
    const text = (el.innerText || "").trim();
    if (!text) continue;
    blocks.push({ el, text });
  }
  return blocks;
}

// ---------------------------------------------------------------------------
// Settings UI (created via DOM API, no innerHTML)
// ---------------------------------------------------------------------------

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

function renderControls(btn) {
  const wrap = btn.closest(".speak-widget");
  if (!wrap) return null;
  const existing = wrap.querySelector(".speak-controls");
  if (existing) return existing;

  const rateSlider = el("input", {
    type: "range",
    min: "0.5",
    max: "2.0",
    step: "0.05",
    class: "speak-rate-slider",
  });
  rateSlider.value = btn.dataset.rate || String(DEFAULT_RATE);
  const rateValue = el("span", { class: "speak-rate-value" },
    parseFloat(rateSlider.value).toFixed(2) + "x");
  rateSlider.addEventListener("input", () => {
    rateValue.textContent = parseFloat(rateSlider.value).toFixed(2) + "x";
    btn.dataset.rate = rateSlider.value;
    if (activeBtn === btn) {
      setActiveRate(parseFloat(rateSlider.value));
    }
  });

  const controls = el("div", { class: "speak-controls", hidden: "" },
    el("label", { class: "speak-control" },
      el("span", { text: "Speed" }),
      rateSlider,
      rateValue),
  );
  wrap.appendChild(controls);
  return controls;
}

function ensureWidgetWrapper(btn) {
  if (btn.parentElement && btn.parentElement.classList.contains("speak-widget")) {
    return btn.parentElement;
  }
  const wrap = el("span", { class: "speak-widget" });
  btn.parentNode.insertBefore(wrap, btn);
  wrap.appendChild(btn);
  return wrap;
}

// ---------------------------------------------------------------------------
// Per-widget state
// ---------------------------------------------------------------------------

let activeBtn = null;

function clearHighlight() {
  document.querySelectorAll(".speak-current").forEach((node) =>
    node.classList.remove("speak-current"));
}

function bindButton(btn) {
  ensureWidgetWrapper(btn);

  const status = btn.querySelector(".speak-status");
  const label = btn.querySelector(".speak-label");
  const initialLabel = label ? label.textContent : "Read aloud";

  function reset() {
    if (label) label.textContent = initialLabel;
    btn.classList.remove("speaking", "loading");
    delete btn.dataset.kind;
    if (status) status.textContent = "";
    clearHighlight();
    activeBtn = null;
  }

  function stopAndReset() {
    stop();
    reset();
  }

  function startSpeaking() {
    let blocks;
    if (btn.dataset.text) {
      blocks = [{ el: null, text: btn.dataset.text }];
    } else if (btn.dataset.target) {
      const target = document.querySelector(btn.dataset.target);
      blocks = target ? [{ el: target, text: target.innerText || "" }] : [];
    } else {
      const root = getArticleRoot();
      blocks = root ? extractBlocks(root) : [];
    }

    if (!blocks.length) {
      reset();
      return;
    }

    activeBtn = btn;
    const voice = btn.dataset.voice || DEFAULT_VOICE;
    const rate = parseFloat(btn.dataset.rate || String(DEFAULT_RATE));

    btn.classList.add("loading");
    if (label) label.textContent = "Loading...";

    function highlightBlock(idx) {
      clearHighlight();
      const block = blocks[idx];
      if (block && block.el) {
        block.el.classList.add("speak-current");
        const r = block.el.getBoundingClientRect();
        if (r.top < 0 || r.bottom > window.innerHeight) {
          block.el.scrollIntoView({ behavior: "smooth", block: "center" });
        }
      }
    }

    const keepParens = btn.dataset.keepParens === "true";

    speak(blocks.map((b) => b.text), voice, {
      rate,
      keepParens,
      onStatus: (msg, kind) => {
        if (status && !btn.classList.contains("speaking")) status.textContent = msg;
        btn.dataset.kind = kind;
      },
      onProgress: (loaded, total) => {
        if (status && total) {
          status.textContent = Math.round((loaded / total) * 100) + "%";
        }
      },
      onChunkStart: () => {
        if (activeBtn !== btn) return;
        btn.classList.remove("loading");
        btn.classList.add("speaking");
        if (label) label.textContent = "Stop";
        if (status) status.textContent = "1/" + blocks.length;
      },
      onBlockStart: (idx) => {
        if (activeBtn !== btn) return;
        highlightBlock(idx);
        if (status) status.textContent = (idx + 1) + "/" + blocks.length;
      },
      onDone: () => {
        if (activeBtn === btn) reset();
      },
      onError: () => {
        if (activeBtn === btn) reset();
      },
    });
  }

  // Expose handlers for the settings panel to trigger auto-restart
  btn._speakStart = startSpeaking;
  btn._speakStop = stopAndReset;

  if (!btn.parentElement.querySelector(".speak-settings-toggle")) {
    const gear = el("button", {
      type: "button",
      class: "speak-settings-toggle",
      "aria-label": "Voice and speed controls",
    }, "⚙");
    btn.parentElement.appendChild(gear);
    gear.addEventListener("click", () => {
      const controls = renderControls(btn);
      if (controls) {
        if (controls.hasAttribute("hidden")) controls.removeAttribute("hidden");
        else controls.setAttribute("hidden", "");
      }
    });
  }

  btn.addEventListener("click", () => {
    if (activeBtn === btn) {
      stopAndReset();
      return;
    }
    if (activeBtn) stop();
    startSpeaking();
  });
}

function init() {
  document.querySelectorAll(".speak-button").forEach(bindButton);
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", init);
} else {
  init();
}
