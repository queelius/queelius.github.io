/**
 * Read-aloud player: plays a pre-rendered narration MP3 and reports which
 * timings span is currently being read, so a widget can highlight along.
 *
 * No in-browser synthesis: the audio and its sentence timings are generated
 * ahead of time (see scripts/gen_audio.py + companion narrate) and shipped as
 * post-bundle resources. This module only plays and tracks.
 */

function setPreservePitch(audio, val) {
  audio.preservesPitch = val;
  audio.mozPreservesPitch = val;
  audio.webkitPreservesPitch = val;
}

export class ReadAloud {
  /**
   * @param {string} audioUrl  URL of the narration mp3
   * @param {Array}  spans     timings.spans: [{start,end,block,sentence,text}]
   * @param {object} opts      { onSpan(i), onState(state), rate }
   */
  constructor(audioUrl, spans, opts = {}) {
    this.spans = Array.isArray(spans) ? spans : [];
    this.onSpan = opts.onSpan || (() => {});
    this.onState = opts.onState || (() => {});
    this._i = -1;
    this.audio = new Audio(audioUrl);
    this.audio.preload = "metadata";
    setPreservePitch(this.audio, true);
    if (opts.rate) this.audio.playbackRate = opts.rate;
    this.audio.addEventListener("timeupdate", () => this._tick());
    this.audio.addEventListener("play", () => this.onState("playing"));
    this.audio.addEventListener("pause", () => this.onState("paused"));
    this.audio.addEventListener("ended", () => {
      this._i = -1;
      this.onSpan(-1);
      this.onState("ended");
    });
  }

  toggle() {
    if (this.audio.paused) this.audio.play();
    else this.audio.pause();
  }

  stop() {
    this.audio.pause();
    this.audio.currentTime = 0;
    this._i = -1;
    this.onSpan(-1);
  }

  setRate(r) {
    this.audio.playbackRate = r;
    setPreservePitch(this.audio, true);
  }

  get playing() {
    return !this.audio.paused;
  }

  _tick() {
    const t = this.audio.currentTime;
    let i = this._i;
    // spans are contiguous and sorted; walk to the covering span.
    while (i + 1 < this.spans.length && t >= this.spans[i + 1].start) i++;
    while (i >= 0 && t < this.spans[i].start) i--;
    if (i !== this._i) {
      this._i = i;
      this.onSpan(i);
    }
  }
}
