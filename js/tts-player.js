/**
 * TTS Player — paragraph-level highlighting synchronized with audio playback.
 * Installed by: narro hugo install
 */
(function () {
  "use strict";

  var player = document.querySelector(".tts-player");
  if (!player) return;

  var audio = player.querySelector("audio");
  var btn = player.querySelector(".tts-play");
  var timeDisplay = player.querySelector(".tts-time");
  var alignUrl = player.dataset.align;

  var alignment = [];
  var blocks = [];     // DOM block elements (p, li, blockquote, ...)
  var activeIdx = -1;
  var rafId = null;

  // --- Initialization ---

  function init() {
    var article = document.querySelector(".content") || document.querySelector("article");
    if (article) {
      blocks = article.querySelectorAll("p, li, blockquote, dd, dt");
    }
    if (alignUrl) {
      fetch(alignUrl)
        .then(function (r) { return r.json(); })
        .then(function (data) { alignment = data; })
        .catch(function () {}); // Degrade gracefully: no highlighting
    }
  }

  // --- Playback ---

  function formatTime(s) {
    var m = Math.floor(s / 60);
    var sec = Math.floor(s % 60);
    return m + ":" + (sec < 10 ? "0" : "") + sec;
  }

  function updateTime() {
    if (!audio.duration) return;
    timeDisplay.textContent =
      formatTime(audio.currentTime) + " / " + formatTime(audio.duration);
  }

  /**
   * Binary search alignment array for the paragraph active at currentTime.
   * Each entry has {paragraph, start, end}. The paragraph field indexes
   * directly into the blocks NodeList.
   */
  function highlightParagraph() {
    if (alignment.length === 0) return;

    var t = audio.currentTime;
    var lo = 0,
      hi = alignment.length - 1,
      mid,
      idx = -1;

    while (lo <= hi) {
      mid = (lo + hi) >> 1;
      if (alignment[mid].start <= t) {
        idx = mid;
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }

    if (idx >= 0 && t > alignment[idx].end) idx = -1;

    if (idx !== activeIdx) {
      if (activeIdx >= 0) {
        var prevEl = blocks[alignment[activeIdx].paragraph];
        if (prevEl) prevEl.classList.remove("tts-active");
      }
      if (idx >= 0) {
        var curEl = blocks[alignment[idx].paragraph];
        if (curEl) curEl.classList.add("tts-active");
      }
      activeIdx = idx;
    }
  }

  /**
   * Animation loop for smooth highlighting (~60fps while playing).
   */
  function tick() {
    updateTime();
    highlightParagraph();
    if (!audio.paused) {
      rafId = requestAnimationFrame(tick);
    }
  }

  function startLoop() {
    if (rafId === null) {
      rafId = requestAnimationFrame(tick);
    }
  }

  function stopLoop() {
    if (rafId !== null) {
      cancelAnimationFrame(rafId);
      rafId = null;
    }
    updateTime();
    highlightParagraph();
  }

  // --- Controls ---

  btn.addEventListener("click", function () {
    if (audio.paused) {
      audio.play();
      btn.textContent = "\u275A\u275A Pause";
    } else {
      audio.pause();
      btn.textContent = "\u25B6 Listen";
    }
  });

  audio.addEventListener("play", startLoop);
  audio.addEventListener("pause", stopLoop);
  audio.addEventListener("seeked", function () {
    updateTime();
    highlightParagraph();
  });

  audio.addEventListener("ended", function () {
    stopLoop();
    btn.textContent = "\u25B6 Listen";
    if (activeIdx >= 0) {
      var el = blocks[alignment[activeIdx].paragraph];
      if (el) el.classList.remove("tts-active");
    }
    activeIdx = -1;
  });

  document.addEventListener("keydown", function (e) {
    if (e.code === "Space" && e.target === document.body) {
      e.preventDefault();
      btn.click();
    }
  });

  init();
})();
