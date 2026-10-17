//! Golden-value tests for the Distortion port.
//!
//! The expected values were captured from the ORIGINAL C++ implementation
//! (`src/effects/DistortionFilter.h`, the signal processing that used to sit in
//! `src/effects/Distortion.cpp`) by `rust/dsp/goldens/capture/distortion.cpp`
//! and are checked in as `rust/dsp/goldens/reference.txt`.
//!
//! Each record carries the settings of one block, and the records of a sample
//! rate are one run of a single filter, so the table and the DC block's rolling
//! average carry from one record to the next - as they did in the dumper.

mod common;

use rustacity_dsp::distortion::{Distortion, Params};

/// Output tolerance: the goldens were produced with the Linux (GCC/glibc)
/// elementary functions while the tests run against this platform's libm, and
/// every table entry is built from several of them.
const OUT_REL: f64 = 1e-4;
const OUT_ABS: f64 = 1e-5;

/// The input the records were captured with: integer arithmetic narrowed to
/// f32, exactly what the dumper generates, so the same samples are reproduced
/// here on any platform.  The values span -1 to 1, which is the whole of the
/// lookup table.  `block` is the record's index within its run.
fn input_sample(i: usize, block: usize) -> f32 {
    (((i * 37 + block * 11) % 129) as i32 - 64) as f32 / 64.0
}

#[test]
fn golden_blocks() {
    let mut checked = 0;
    let mut rate = 0.0;
    let mut block = 0;
    let mut distortion: Option<Distortion> = None;

    for r in common::load() {
        if r.kind != "dis-block" {
            continue;
        }

        let record_rate = r.f(0);
        if record_rate != rate {
            // A record's index restarts where the filter is created, which is
            // what the dumper does when it moves to the next sample rate.
            rate = record_rate;
            block = 0;
            distortion = Some(Distortion::new(rate as f32));
        }

        let params = Params {
            table_choice: r.i(1) as i32,
            dc_block: r.i(2) != 0,
            threshold_db: r.f(3),
            noise_floor: r.f(4),
            param1: r.f(5),
            param2: r.f(6),
            repeats: r.i(7) as i32,
        };
        let len = r.i(8) as usize;
        let stride = r.i(9) as usize;
        assert!(stride >= 1, "dis-block stride");

        let printed = (len + stride - 1) / stride;
        assert_eq!(r.fields.len(), 10 + printed, "dis-block sample count");

        let input: Vec<f32> = (0..len).map(|i| input_sample(i, block)).collect();
        let mut output = vec![0.0f32; len];
        let state = distortion.as_mut().expect("a filter for each sample rate");
        state.process_block(params, &input, &mut output);

        for (n, i) in (0..len).step_by(stride).enumerate() {
            common::close(
                f64::from(output[i]),
                r.f(10 + n),
                OUT_REL,
                OUT_ABS,
                &format!(
                    "dis-block {} Hz, record {} (table {}, DC block {}, threshold {} dB, \
                     noise floor {}, params {} {} x{}), sample {}",
                    rate,
                    block,
                    params.table_choice,
                    params.dc_block,
                    params.threshold_db,
                    params.noise_floor,
                    params.param1,
                    params.param2,
                    params.repeats,
                    i
                ),
            );
        }

        block += 1;
        checked += 1;
    }

    assert_eq!(checked, 90, "every captured block record is checked");
}
