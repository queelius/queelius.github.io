/**
 * Read-aloud widget: bind any element with class="speak-button" that carries
 * data-audio (mp3) and data-timings (sentence timings JSON).
 *
 * Plays the pre-rendered narration and, as each timings span is read, adds the
 * `speak-current` class to the article block it belongs to. Highlighting is at
 * block granularity (it only toggles a class, so in-article links and
 * formatting are preserved). No in-browser synthesis.
 */

import { ReadAloud } from "/speak/lib.js";

const SPEAKABLE_SELECTOR = "p, li, blockquote, dt, dd, h2, h3, h4, h5, h6";
const SKIP_SELECTOR =
  "pre, code, .speak-widget, .speak-button, .speak-controls, figure, .footnote-backref, sup.footnote-ref";
const RATES = [1, 1.25, 1.5, 0.75];

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
  for (const node of root.querySelectorAll(SPEAKABLE_SELECTOR)) {
    if (node.closest(SKIP_SELECTOR)) continue;
    // Skip a block nested inside another speakable block (list wrappers).
    let p = node.parentElement, nested = false;
    while (p && p !== root) {
      if (p.matches && p.matches(SPEAKABLE_SELECTOR) && !p.closest(SKIP_SELECTOR)) {
        nested = true;
        break;
      }
      p = p.parentElement;
    }
    if (nested) continue;
    const text = (node.innerText || "").trim();
    if (text) blocks.push({ el: node, text });
  }
  return blocks;
}

const _norm = (s) => (s || "").toLowerCase().replace(/\s+/g, " ").trim();

/**
 * Map each timings span to a DOM block by text. Spans and blocks are both in
 * reading order, so a moving cursor keeps the search local and monotonic; the
 * extractor's block segmentation (markdown paragraphs) need not match the DOM's
 * (rendered elements, e.g. list items), because matching is by content.
 */
function mapSpansToBlocks(spans, blocks) {
  const out = new Array(spans.length).fill(null);
  let cur = 0;
  for (let i = 0; i < spans.length; i++) {
    const needle = _norm(spans[i].text).replace(/[^a-z0-9 ]/g, "").slice(0, 30);
    let found = -1;
    for (let j = cur; j < blocks.length; j++) {
      if (_norm(blocks[j].text).replace(/[^a-z0-9 ]/g, "").includes(needle)) {
        found = j;
        break;
      }
    }
    if (found >= 0) {
      out[i] = blocks[found].el;
      cur = found;
    } else if (cur < blocks.length) {
      out[i] = blocks[cur].el; // fallback: stay on the current block
    }
  }
  return out;
}

function bindButton(btn) {
  const audioUrl = btn.dataset.audio;
  const timingsUrl = btn.dataset.timings;
  if (!audioUrl || !timingsUrl) return;

  const label = btn.querySelector(".speak-label");
  const status = btn.querySelector(".speak-status");
  const initialLabel = label ? label.textContent : "Read aloud";

  let player = null;
  let spanBlocks = [];
  let ready = false;

  function clearHighlight() {
    document
      .querySelectorAll(".speak-current")
      .forEach((n) => n.classList.remove("speak-current"));
  }

  function highlight(i) {
    clearHighlight();
    const target = i >= 0 ? spanBlocks[i] : null;
    if (!target) return;
    target.classList.add("speak-current");
    const r = target.getBoundingClientRect();
    if (r.top < 0 || r.bottom > window.innerHeight) {
      target.scrollIntoView({ behavior: "smooth", block: "center" });
    }
  }

  function onState(state) {
    if (state === "playing") {
      btn.classList.add("speaking");
      if (label) label.textContent = "Stop";
    } else {
      btn.classList.remove("speaking");
      if (label) label.textContent = initialLabel;
      if (state === "ended") clearHighlight();
    }
  }

  async function ensurePlayer() {
    if (ready) return true;
    if (status) status.textContent = "Loading...";
    try {
      const timings = await (await fetch(timingsUrl)).json();
      const root = getArticleRoot();
      const blocks = root ? extractBlocks(root) : [];
      spanBlocks = mapSpansToBlocks(timings.spans || [], blocks);
      player = new ReadAloud(audioUrl, timings.spans, {
        onSpan: highlight,
        onState,
        rate: parseFloat(btn.dataset.rate || "1"),
      });
      ready = true;
      if (status) status.textContent = "";
      return true;
    } catch (e) {
      if (status) status.textContent = "Audio unavailable";
      console.warn("read-aloud: failed to load", e);
      return false;
    }
  }

  function makeSpeedControl() {
    const wrap = btn.closest(".speak-widget") || btn.parentElement;
    if (!wrap || wrap.querySelector(".speak-settings-toggle")) return;
    let ri = 0;
    const value = el("span", { class: "speak-rate-value" }, "1.00x");
    const slider = el("input", {
      type: "range", min: "0.5", max: "2.0", step: "0.05", class: "speak-rate-slider",
    });
    slider.value = "1";
    slider.addEventListener("input", () => {
      const r = parseFloat(slider.value);
      value.textContent = r.toFixed(2) + "x";
      btn.dataset.rate = slider.value;
      if (player) player.setRate(r);
    });
    const controls = el(
      "div", { class: "speak-controls", hidden: "" },
      el("label", { class: "speak-control" }, el("span", { text: "Speed" }), slider, value),
    );
    const gear = el(
      "button",
      { type: "button", class: "speak-settings-toggle", "aria-label": "Playback speed" },
      "⚙",
    );
    wrap.appendChild(gear);
    wrap.appendChild(controls);
    gear.addEventListener("click", () => {
      if (controls.hasAttribute("hidden")) controls.removeAttribute("hidden");
      else controls.setAttribute("hidden", "");
    });
  }

  // Wrap the button in a positioning container for the popover, if needed.
  if (!(btn.parentElement && btn.parentElement.classList.contains("speak-widget"))) {
    const wrap = el("span", { class: "speak-widget" });
    btn.parentNode.insertBefore(wrap, btn);
    wrap.appendChild(btn);
  }
  makeSpeedControl();

  btn.addEventListener("click", async () => {
    if (!(await ensurePlayer())) return;
    player.toggle();
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
