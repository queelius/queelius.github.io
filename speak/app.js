/**
 * /speak/ page: standalone TTS playground using the shared speak library.
 */

import { speak, stop, VOICES, isSpeaking } from "/speak/lib.js";

const SAMPLE_TEXT =
  "The blog you are reading is rendered by a static site generator. " +
  "These words are being spoken by a small neural network running locally in your browser, with no server in between.";

const els = {
  status: document.getElementById("tts-status"),
  voice: document.getElementById("tts-voice"),
  text: document.getElementById("tts-text"),
  speak: document.getElementById("tts-speak"),
  progress: document.getElementById("tts-progress"),
  progressBar: document.getElementById("tts-progress-bar"),
};

let busy = false;

function setStatus(text, kind = "info") {
  els.status.textContent = text;
  els.status.dataset.kind = kind;
}

function setProgress(loaded, total) {
  if (!total) {
    els.progress.hidden = true;
    return;
  }
  els.progress.hidden = false;
  const pct = Math.min(100, Math.round((loaded / total) * 100));
  els.progressBar.style.width = pct + "%";
  els.progressBar.textContent = pct + "%";
}

function populateVoices() {
  for (const v of VOICES) {
    const opt = document.createElement("option");
    opt.value = v.id;
    opt.textContent = v.label;
    els.voice.appendChild(opt);
  }
  els.text.value = SAMPLE_TEXT;
}

function onSpeakClick() {
  if (busy) {
    stop();
    busy = false;
    els.speak.textContent = "Speak";
    setStatus("Stopped.", "idle");
    setProgress(0, 0);
    return;
  }
  const text = els.text.value.trim();
  if (!text) return;
  busy = true;
  els.speak.textContent = "Stop";

  speak(text, els.voice.value, {
    onStatus: setStatus,
    onProgress: setProgress,
    onChunkStart: () => setStatus("Playing...", "ready"),
    onDone: (n) => {
      setStatus(`Done. ${n} chunk${n !== 1 ? "s" : ""} synthesized.`, "ready");
      busy = false;
      els.speak.textContent = "Speak";
      setProgress(0, 0);
    },
    onError: () => {
      busy = false;
      els.speak.textContent = "Speak";
      setProgress(0, 0);
    },
  });
}

els.speak.addEventListener("click", onSpeakClick);
els.text.addEventListener("keydown", (e) => {
  if ((e.metaKey || e.ctrlKey) && e.key === "Enter") onSpeakClick();
});

populateVoices();
setStatus("Pick a voice and click Speak. First synthesis downloads the voice (~22 MB for 'low').", "idle");
