//! Golden-value tests for the EBU R128 port.
//!
//! The expected values were captured from the ORIGINAL C++
//! implementation (`src/effects/EBUR128.cpp`) by the temporary harness
//! `src/goldendump.cpp` and are checked in as
//! `rust/dsp/goldens/reference.txt`.

mod common;

use rustacity_dsp::ebur128::{calc_weighting_filter, Ebur128};

/// Weighted-sample tolerance: `process_one` returns f32, so the
/// double-precision K-weighting chain is quantized on every sample.
const OUT_REL: f64 = 1e-4;
const OUT_ABS: f64 = 1e-5;

/// Gated loudness tolerance: a block landing in a neighbouring
/// histogram bin shifts the result by one bin step, which is about
/// 2.4e-4 in relative terms.
const LOUDNESS_REL: f64 = 1e-3;
const LOUDNESS_ABS: f64 = 1e-6;

#[test]
fn golden_process_weighting() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "process-weighting" {
            continue;
        }
        let fs = r.f(0);
        let len = r.i(1) as usize;

        // Driven exactly like EBUR128::ProcessSampleFromChannel: the
        // sample rate adapted K-weighting pair, chained with a float
        // round trip per section.
        let mut filters = calc_weighting_filter(fs);
        let mut noise = common::Noise::new(20261017);
        for i in 0..len {
            let x = noise.next_sample();
            let mut value = f64::from(filters[0].process_one(x));
            value = f64::from(filters[1].process_one(value as f32));
            common::close(
                value,
                r.f(2 + i),
                OUT_REL,
                OUT_ABS,
                &format!("process-weighting fs={} sample={}", fs, i),
            );
        }
        checked += 1;
    }
    assert!(
        checked >= 2,
        "only {} process-weighting records checked",
        checked
    );
}

#[test]
fn golden_ebur128() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "ebur128" {
            continue;
        }
        let rate = r.f(0);
        let channels = r.i(1) as usize;
        let nsamples = r.i(2) as usize;
        let seed = r.i(3) as u32;
        let mode = r.s(4).to_owned();
        let want = r.f(5);
        let silent = mode == "silent";

        let mut meter = Ebur128::new(rate, channels);
        meter.initialize();
        let mut noise = common::Noise::new(seed);
        for _ in 0..nsamples {
            let x0 = if silent { 0.0 } else { noise.next_sample() };
            meter.process_sample_from_channel(x0, 0);
            if channels > 1 {
                let x1 = if silent { 0.0 } else { noise.next_sample() };
                meter.process_sample_from_channel(x1, 1);
            }
            meter.next_sample();
        }

        common::close(
            meter.integrative_loudness(),
            want,
            LOUDNESS_REL,
            LOUDNESS_ABS,
            &format!(
                "ebur128 rate={} channels={} samples={} seed={} mode={}",
                rate, channels, nsamples, seed, mode
            ),
        );
        checked += 1;
    }
    assert!(checked >= 5, "only {} ebur128 records checked", checked);
}

#[test]
fn batch_processing_matches_goldens() {
    // The block API used by EffectLoudness must produce exactly the
    // same measurement as the per-sample API used in the goldens.
    let rate = 48000.0;
    let channels = 2;
    let len = 60000;
    let mut noise = common::Noise::new(20261017);
    let left: Vec<f32> = (0..len).map(|_| noise.next_sample()).collect();
    let right: Vec<f32> = (0..len).map(|_| noise.next_sample()).collect();

    let mut block = Ebur128::new(rate, channels);
    block.initialize();
    block.process_block(&[&left, &right]);

    let mut per_sample = Ebur128::new(rate, channels);
    per_sample.initialize();
    for i in 0..len {
        per_sample.process_sample_from_channel(left[i], 0);
        per_sample.process_sample_from_channel(right[i], 1);
        per_sample.next_sample();
    }

    assert_eq!(
        block.integrative_loudness(),
        per_sample.integrative_loudness()
    );
}
