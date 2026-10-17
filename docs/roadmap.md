# Roadmap

## Rust-first

Audio processing is moving to Rust one unit at a time; the C++ tree is the
shell being retired, and it is not extended. The plan that makes each move
safe is in [`docs/architecture.md`](architecture.md), the ledger of what has
moved is in [`docs/port-status.md`](port-status.md), and the numbers that keep
a port honest are in [`docs/goldens.md`](goldens.md).

The next enabler is an **FFT in Rust**: `NoiseReduction`, `NoiseRemoval`,
`Equalization`/`Equalization48x` and `ClickRemoval` all work in the frequency
domain, and the crate currently has no dependencies. That decision - write one
or take one - comes before those four.

## The extension engine

The point of moving the DSP to Rust is the extension engine named on the
front page: a Rust-based layer for adding functionality to the editor. The C
ABI, the panic policy and the golden-value discipline exist to make a Rust
component something that can be added to the application without fear. The
extension API itself does not exist yet.

## What is not going anywhere

`lib-src/` is vendored third-party code and is never touched. The wxWidgets
effect framework stays C++ by design - it is the interface the user sees, not
signal processing. And the classes in `src/effects/` stay even after their
maths has moved, because they are what the application's effect list, dialogs
and undo history are built from.
