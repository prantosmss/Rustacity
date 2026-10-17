# Rustacity

**An [Audacity](https://www.audacityteam.org) v2.4.1 fork created for the Rustaceans.**

Rustacity is an easy-to-use, multi-track audio editor and recorder for Windows, Mac OS X, GNU/Linux and other operating systems. It keeps everything you know about Audacity, and will grow a Rust-based extension engine for adding new functionality.

- **Recording** from any real, or virtual audio device that is available to the host system.
- **Export / Import** a wide range of audio formats, extendible with FFmpeg.
- **High quality** using 32-bit float audio processing.
- **Plug-ins** Support for multiple audio plug-in formats, including VST, LV2, AU.
- **Macros** for chaining commands and batch processing.
- **Scripting** in Python, Perl, or any language that supports named pipes.
- **Nyquist** Very powerful built-in scripting language that may also be used to create plug-ins.
- **Editing** multi-track editing with sample accuracy and arbitrary sample rates.
- **Accessibility** for VI users.
- **Analysis and visualization** tools to analyze audio, or other signal data.

## Roadmap

- **Rust extension engine** — a Rust-based plugin/extension layer for adding new functionality to the editor.

## Getting Started

Rustacity is built from source. The project ships GitHub Actions workflows (`.github/workflows/`) and `appveyor.yml` that produce CI builds.

Because Rustacity is a fork of Audacity 2.4.1, end-user documentation, forums, and developer references are still available from the upstream [Audacity website](https://www.audacityteam.org/), [Audacity Forum](https://forum.audacityteam.org/), and [Audacity Wiki](https://wiki.audacityteam.org/wiki/For_Developers).

### The Rust DSP core

Parts of the audio processing are implemented in Rust, in the `rustacity-dsp` crate under `rust/dsp/`. The build invokes `cargo` and links the crate as a static library, so a Rust toolchain is required in addition to the C++ one; the C interface between the two halves is `rust/dsp/include/rustacity_dsp.h`.

The ported code is checked against reference values captured from the original C++ implementation (`rust/dsp/goldens/reference.txt`):

```
cd rust
cargo test --workspace
```

## Releasing

Pushing an annotated tag named `rustacity-<version>` builds all four
platforms (Windows 32/64-bit, Ubuntu and macOS) and, once those builds
and the Rust tests pass, publishes a GitHub release with the resulting
zips attached.

The release is published with the maintainer's own account instead of the
workflow's `GITHUB_TOKEN`, because a release created with `GITHUB_TOKEN`
is attributed to `github-actions[bot]`. This requires a repository
secret:

| Secret | Value |
| --- | --- |
| `RELEASE_TOKEN` | fine-grained personal access token with **Contents: Read and write** on this repository |

To create it: *Settings → Developer settings → Personal access tokens →
Fine-grained tokens → Generate new token*, set the resource owner to
`prantosmss`, choose *Only select repositories → Rustacity*, grant the
repository permission *Contents: Read and write*, and then add the token
as the `RELEASE_TOKEN` secret under *Settings → Secrets and variables →
Actions → New repository secret*. The release job fails with an
explanation if the secret is missing or its token has expired.
