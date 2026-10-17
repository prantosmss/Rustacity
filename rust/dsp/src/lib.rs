//! DSP core of Rustacity.
//!
//! Pure-math audio DSP ported from the original Audacity C++ sources,
//! exposed to the application through the C ABI declared in
//! `include/rustacity_dsp.h`.  Zero runtime dependencies.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

// The Biquad filter and EBU R128 loudness modules land with their ports.

#[cfg(test)]
mod tests {
    #[test]
    fn workspace_smoke() {
        assert_eq!(2 + 2, 4);
    }
}
