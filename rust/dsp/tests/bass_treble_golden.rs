//! Golden-value tests for the EffectBassTreble shelf filters.
//!
//! The expected values were captured from the ORIGINAL C++ implementation
//! (`src/effects/BassTrebleFilter.h`, the signal processing that used to
//! sit in `src/effects/BassTreble.cpp`) by
//! `rust/dsp/goldens/capture/basstreble.cpp` and are checked in as
//! `rust/dsp/goldens/reference.txt`.

mod common;

use rustacity_dsp::bass_treble::{BassTreble, Shelf};

/// Coefficient tolerance: the goldens were produced with the Linux
/// (GCC/glibc) elementary functions while the tests run against this
/// platform's libm, so a few ulps of difference are expected.
const COEFF_REL: f64 = 1e-9;
const COEFF_ABS: f64 = 1e-12;

/// Filtered-output tolerance: the C++ narrows every sample to f32 and the
/// coefficients it does so with differ in their last bits.
const OUT_REL: f64 = 1e-4;
const OUT_ABS: f64 = 1e-5;

/// The input the `bt-block` records were captured with: integer arithmetic
/// narrowed to f32, exactly as the dumper generates it, so the same samples
/// are reproduced here on any platform.
fn block_input(len: usize) -> Vec<f32> {
    (0..len)
        .map(|i| (((i * 37) % 101) as i32 - 50) as f32 / 64.0f32)
        .collect()
}

fn shelf_of(kind: i64) -> Shelf {
    match kind {
        0 => Shelf::Bass,
        1 => Shelf::Treble,
        other => panic!("unknown shelf {}", other),
    }
}

#[test]
fn golden_coefficients() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "bt-coeffs" {
            continue;
        }
        let rate = r.f(0);
        let kind = r.i(1);
        let hz = r.f(2);
        let slope = r.f(3);
        let gain_db = r.f(4);

        let c = BassTreble::coefficients(shelf_of(kind), hz, slope, gain_db, rate);
        let got = [c.a0, c.a1, c.a2, c.b0, c.b1, c.b2];
        let names = ["a0", "a1", "a2", "b0", "b1", "b2"];
        for i in 0..6 {
            common::close(
                got[i],
                r.f(5 + i),
                COEFF_REL,
                COEFF_ABS,
                &format!(
                    "bt-coeffs {} Hz, shelf {}, {} dB, {}: {}",
                    rate, kind, gain_db, names[i], got[i]
                ),
            );
        }
        checked += 1;
    }
    assert_eq!(checked, 42, "every captured coefficient record is checked");
}

#[test]
fn golden_blocks() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "bt-block" {
            continue;
        }
        let rate = r.f(0);
        let bass_db = r.f(1);
        let treble_db = r.f(2);
        let gain_db = r.f(3);
        let len = r.i(4) as usize;
        assert_eq!(r.fields.len(), 5 + len, "bt-block sample count");

        let input = block_input(len);
        let mut state = BassTreble::new(rate as f32);
        state.set_parameters(bass_db, treble_db, gain_db);

        for (i, &x) in input.iter().enumerate() {
            let got = f64::from(state.process_one(x));
            common::close(
                got,
                r.f(5 + i),
                OUT_REL,
                OUT_ABS,
                &format!(
                    "bt-block sample {} of {} Hz, bass {}, treble {}, gain {}",
                    i, rate, bass_db, treble_db, gain_db
                ),
            );
        }
        checked += 1;
    }
    assert_eq!(checked, 4, "every captured block record is checked");
}
