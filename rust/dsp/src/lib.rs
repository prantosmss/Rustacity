//! DSP core of Rustacity.
//!
//! Pure-math audio DSP ported from the original Audacity C++ sources,
//! exposed to the application through the C ABI declared in
//! `include/rustacity_dsp.h`.  Zero runtime dependencies.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

pub mod biquad;

mod ffi;
