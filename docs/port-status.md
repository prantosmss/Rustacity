# Port status

The audio maths moves from `src/effects/` to `rust/dsp/src/`, one unit at a
time, under the rule in [`docs/architecture.md`](architecture.md): a port is
finished only when the C++ implementation is deleted. This is the ledger.

## Ported

| unit | C++ maths it replaced | Rust | reference values |
| --- | --- | --- | --- |
| biquad filter design and processing | `src/effects/Biquad.cpp` (the filter `ScienFilter` uses) | `rust/dsp/src/biquad.rs` | `design`, `chebypoly`, `process-design`, `process-weighting` |
| EBU R128 loudness measurement | `src/effects/EBUR128.cpp` (the meter `Loudness` and `Contrast` call) | `rust/dsp/src/ebur128.rs` | `ebur128` |
| bass and treble shelves | `src/effects/BassTreble.cpp` | `rust/dsp/src/bass_treble.rs` | `bt-coeffs`, `bt-block` |
| phaser allpass chain | `src/effects/Phaser.cpp` | `rust/dsp/src/phaser.rs` | `ph-block` |
| wahwah | `src/effects/Wahwah.cpp` | `rust/dsp/src/wahwah.rs` | `wah-block` |
| distortion waveshaper | `src/effects/Distortion.cpp` | `rust/dsp/src/distortion.rs` | `dis-block` |

In every case the class in `src/effects/` stays. It owns the dialog, the
parameter marshalling, the track iteration and the buffer copying, and calls a
Rust handle per channel; the maths exists in exactly one place. The record
formats in `rust/dsp/goldens/reference.txt` name each unit's original C++
source, which is the record of what was deleted.

The four realtime effects are done and the set is closed: `SupportsRealtime`
is overridden by exactly four built-in effects - BassTreble, Phaser, Wahwah
and Distortion - so no further effect can be a realtime candidate without the
framework itself changing.

## Not ported

**The effect framework stays C++.** `Effect.cpp`, `EffectUI.cpp`,
`EffectManager.cpp`, `LoadEffects.cpp` and `RealtimeEffectManager.cpp` are
bound to wxWidgets and to the application's event and dialog machinery. They
are the shell the Rust core is called from, not audio maths.

**Needs an FFT in Rust first.** `NoiseReduction`, `NoiseRemoval`,
`Equalization` with `Equalization48x`, and `ClickRemoval` all work in the
frequency domain. The crate has no dependencies today, so this needs a
decision - write an FFT in the crate, or take a dependency - before any of
them can move.

**Too little maths to be worth a boundary.** `Amplify`, `Normalize`, `Fade`,
`Invert`, `Reverse`, `Repeat`, `Silence`, `StereoToMono` and the tone, click
and noise generators are a few lines each; a handle, an ABI entry point and a
golden pass would cost more than they save.

**Offline only.** `Reverb`, `Echo` and `Compressor` have no realtime path -
the framework renders them offline - so porting them buys less than a realtime
effect does.
