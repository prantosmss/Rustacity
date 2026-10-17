//! Golden-value tests for the Wahwah port.
//!
//! The expected values were captured from the ORIGINAL C++ implementation
//! (`src/effects/WahwahFilter.h`, the signal processing that used to sit in
//! `src/effects/Wahwah.cpp`) by `rust/dsp/goldens/capture/wahwah.cpp` and are
//! checked in as `rust/dsp/goldens/reference.txt`.
//!
//! Each record carries the parameters of one block, and the records of a sample
//! rate are one run of a single filter, so the state carries from one record to
//! the next - as it did in the dumper.

mod common;

use rustacity_dsp::wahwah::{Params, Wahwah};

/// Output tolerance: the goldens were produced with the Linux (GCC/glibc)
/// elementary functions while the tests run against this platform's libm, and
/// the recursive filter carries a few ulps from sample to sample.
const OUT_REL: f64 = 1e-4;
const OUT_ABS: f64 = 1e-5;

/// The input the records were captured with: integer arithmetic narrowed to
/// f32, exactly what the dumper generates, so the same samples are reproduced
/// here on any platform.  `block` is the record's index within its run.
fn input_sample(i: usize, block: usize) -> f32 {
    (((i * 37 + block * 11) % 101) as i32 - 50) as f32 / 64.0f32
}

#[test]
fn golden_blocks() {
    let mut checked = 0;
    let mut rate = 0.0;
    let mut block = 0;
    let mut wahwah: Option<Wahwah> = None;

    for r in common::load() {
        if r.kind != "wah-block" {
            continue;
        }

        let record_rate = r.f(0);
        if record_rate != rate {
            // A record's index restarts where the filter is created, which is
            // what the dumper does when it moves to the next sample rate.
            rate = record_rate;
            block = 0;
            wahwah = Some(Wahwah::new(rate as f32));
        }

        let params = Params {
            freq: r.f(1),
            phase_deg: r.f(2),
            depth: r.i(3) as i32,
            res: r.f(4),
            freq_ofs: r.i(5) as i32,
            out_gain_db: r.f(6),
        };
        let len = r.i(7) as usize;
        let stride = r.i(8) as usize;
        assert!(stride >= 1, "wah-block stride");

        let printed = (len + stride - 1) / stride;
        assert_eq!(r.fields.len(), 9 + printed, "wah-block sample count");

        let input: Vec<f32> = (0..len).map(|i| input_sample(i, block)).collect();
        let mut output = vec![0.0f32; len];
        let state = wahwah.as_mut().expect("a filter for each sample rate");
        state.process_block(params, &input, &mut output);

        for (n, i) in (0..len).step_by(stride).enumerate() {
            common::close(
                f64::from(output[i]),
                r.f(9 + n),
                OUT_REL,
                OUT_ABS,
                &format!(
                    "wah-block {} Hz, record {} ({} Hz LFO, depth {}, res {}), sample {}",
                    rate, block, params.freq, params.depth, params.res, i
                ),
            );
        }

        block += 1;
        checked += 1;
    }

    assert_eq!(checked, 18, "every captured block record is checked");
}
