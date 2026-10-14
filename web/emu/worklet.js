// SPDX-License-Identifier: GPL-3.0-only
// One firmware instance owns both the physical UI and audio, on the audio thread.
class EmuEngine {
  constructor(module) {
    if (WebAssembly.Module.imports(module).length) throw new Error('Unexpected emulator runtime imports.');
    this.w = new WebAssembly.Instance(module, {}).exports;
    this.w._initialize?.();
    this.w.jam_emu_init();
    this.frames = this.w.jam_emu_frames();
    if (this.w.jam_emu_rate() !== sampleRate) throw new Error('The emulator needs 44.1 kHz audio.');
    this.audio = new Float32Array(this.w.memory.buffer, this.w.jam_emu_render(), this.frames * 2);
    this.cursor = this.frames;
  }
  command({ type, args = [] }) {
    const w = this.w;
    switch (type) {
      case 'key': w.jam_emu_key(args[0], args[1]); break;
      case 'button': w.jam_emu_button(args[0], args[1]); break;
      case 'encoder': w.jam_emu_encoder(args[0], args[1]); break;
      case 'master': w.jam_emu_master(args[0]); break;
      case 'masterStep': w.jam_emu_master(w.jam_emu_value(16, 0) + args[0]); break;
      case 'release': w.jam_emu_release(); break;
      case 'panic': w.jam_emu_panic(); break;
      default: throw new Error('Unknown panel input.');
    }
  }
  snapshot() {
    const w = this.w, value = (field, index = 0) => w.jam_emu_value(field, index);
    const bytes = new Uint8Array(w.memory.buffer);
    let sound = '', pointer = w.jam_emu_sound_name();
    for (let i = 0; i < 80 && bytes[pointer + i]; i++) sound += String.fromCharCode(bytes[pointer + i]);
    return {
      selected: value(0), playing: !!value(1), recording: value(2), armed: !!value(3),
      keys: value(4), buttons: value(5), home: !!value(6), homeControls: !!value(20), homeView: value(21), page: value(7), tempo: value(8),
      octave: value(9), frames: value(10), layer: value(11),
      groove: [0, 1, 2, 3].map(i => value(22, i)),
      macros: [0, 1, 2, 3].map(i => value(12, i)), macroAvailable: !!value(13),
      held: value(14), engine: value(15), master: value(16), screenVersion: value(17), sound,
      events: [0, 1, 2, 3].map(track => value(19, track)),
      leds: Array.from(new Uint8Array(w.memory.buffer, w.jam_emu_leds(), 41)),
    };
  }
  render(left, right) {
    for (let i = 0; i < left.length; i++) {
      if (this.cursor >= this.frames) { this.w.jam_emu_render(); this.cursor = 0; }
      left[i] = this.audio[this.cursor * 2]; right[i] = this.audio[this.cursor * 2 + 1];
      this.cursor++;
    }
  }
}

class JamEmuProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.engine = new EmuEngine(options.processorOptions.module);
    this.queue = []; this.panicPending = false; this.sequence = 0; this.ackPending = false;
    this.elapsed = 0; this.peak = 0; this.screenVersion = -1;
    // Only one frame can be in flight. The page returns its buffer after drawing.
    this.frameBuffer = new ArrayBuffer(240 * 240 * 4);
    this.port.onmessage = ({ data }) => {
      if (data.type === 'frame-return') {
        if (data.buffer instanceof ArrayBuffer && data.buffer.byteLength === 240 * 240 * 4) this.frameBuffer = data.buffer;
      } else if (data.type === 'panic') {
        this.queue.length = 0; this.panicPending = true; this.sequence = data.sequence; this.ackPending = true;
      } else if (data.type === 'commands') {
        this.sequence = data.sequence; this.ackPending = true;
        if (!Array.isArray(data.commands) || data.commands.length > 64 - this.queue.length) {
          this.queue.length = 0; this.panicPending = true;
          this.port.postMessage({ warning: 'Inputs arrived too quickly. Notes released; press Play to continue.' });
        } else this.queue.push(...data.commands);
      }
    };
    this.publish();
  }
  publish() {
    const data = { state: this.engine.snapshot(), peak: this.peak };
    if (this.ackPending && !this.queue.length && !this.panicPending) {
      data.ack = this.sequence; this.ackPending = false;
    }
    const version = this.engine.w.jam_emu_screen_version();
    if (this.frameBuffer && version !== this.screenVersion) {
      const source = new Uint8Array(this.engine.w.memory.buffer, this.engine.w.jam_emu_screen(), 240 * 240 * 4);
      new Uint8Array(this.frameBuffer).set(source);
      data.frame = this.frameBuffer; this.frameBuffer = null; this.screenVersion = version;
      this.port.postMessage(data, [data.frame]);
    } else this.port.postMessage(data);
    this.peak = 0; this.elapsed = 0;
  }
  process(_input, outputs) {
    const [left, right] = outputs[0];
    try {
      let changed = false;
      if (this.panicPending) {
        this.engine.command({ type: 'panic' }); this.panicPending = false; changed = true;
      }
      // A physical edge gets an audio quantum before the next edge. Otherwise
      // a short key tap can disappear, or two PLAY presses can read the same
      // pending firmware transport state before the mixer applies it.
      if (this.queue.length) { this.engine.command(this.queue.shift()); changed = true; }
      this.engine.render(left, right);
      for (let i = 0; i < left.length; i++) this.peak = Math.max(this.peak, Math.abs(left[i]), Math.abs(right[i]));
      this.elapsed += left.length;
      if (changed || this.elapsed >= 1536) this.publish();
      return true;
    } catch (error) {
      left.fill(0); right.fill(0);
      this.port.postMessage({ error: error.message });
      return false;
    }
  }
}
registerProcessor('jambox-emu', JamEmuProcessor);
