//! Golden-value tests for the Biquad port.
//!
//! The expected values were captured from the ORIGINAL C++
//! implementation (`src/effects/Biquad.cpp`) by the temporary harness
//! `src/goldendump.cpp` and are checked in as
//! `rust/dsp/goldens/reference.txt`.

mod common;

use rustacity_dsp::biquad::{
    calc_butterworth_filter, calc_chebyshev_type1_filter, calc_chebyshev_type2_filter, cheby_poly,
    Biquad,
};

/// Coefficient tolerance: the goldens were produced with the Linux
/// (GCC/glibc) elementary functions while the tests run against this
/// platform's libm, so a few ulps of difference are expected.
const COEFF_REL: f64 = 1e-9;
const COEFF_ABS: f64 = 1e-12;

/// Filtered-output tolerance: `process_one` returns f32 and small
/// coefficient differences are rounded to f32 along the way.
const OUT_REL: f64 = 1e-4;
const OUT_ABS: f64 = 1e-5;

fn design(kind: &str, order: i32, fn_hz: f64, fc: f64, ripple: f64, subtype: i32) -> Vec<Biquad> {
    match kind {
        "butter" => calc_butterworth_filter(order, fn_hz, fc, subtype),
        "cheby1" => calc_chebyshev_type1_filter(order, fn_hz, fc, ripple, subtype),
        "cheby2" => calc_chebyshev_type2_filter(order, fn_hz, fc, ripple, subtype),
        other => panic!("unknown filter kind {:?}", other),
    }
}

#[test]
fn golden_designs() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "design" {
            continue;
        }
        let kind = r.s(0);
        let order = r.i(1) as i32;
        let fn_hz = r.f(2);
        let fc = r.f(3);
        let ripple = r.f(4);
        let subtype = r.i(5) as i32;
        let n = r.i(6) as usize;

        let sections = design(kind, order, fn_hz, fc, ripple, subtype);
        assert_eq!(
            sections.len(),
            n,
            "section count for {} order {}",
            kind,
            order
        );
        for i in 0..n {
            for c in 0..5 {
                let got = match c {
                    0 => sections[i].numer[0],
                    1 => sections[i].numer[1],
                    2 => sections[i].numer[2],
                    3 => sections[i].denom[0],
                    _ => sections[i].denom[1],
                };
                let want = r.f(7 + i * 5 + c);
                common::close(
                    got,
                    want,
                    COEFF_REL,
                    COEFF_ABS,
                    &format!(
                        "design {} order={} fn={} fc={} ripple={} sub={} section={} coeff={}",
                        kind, order, fn_hz, fc, ripple, subtype, i, c
                    ),
                );
            }
        }
        checked += 1;
    }
    assert!(checked >= 100, "only {} design records checked", checked);
}

#[test]
fn golden_cheby_poly() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "chebypoly" {
            continue;
        }
        let order = r.i(0) as i32;
        let x = r.f(1);
        let want = r.f(2);
        common::close(
            cheby_poly(order, x),
            want,
            COEFF_REL,
            COEFF_ABS,
            &format!("chebypoly order={} x={}", order, x),
        );
        checked += 1;
    }
    assert!(checked >= 10, "only {} chebypoly records checked", checked);
}

#[test]
fn golden_process_design() {
    let mut checked = 0;
    for r in common::load() {
        if r.kind != "process-design" {
            continue;
        }
        let kind = r.s(0);
        let order = r.i(1) as i32;
        let fn_hz = r.f(2);
        let fc = r.f(3);
        let ripple = r.f(4);
        let subtype = r.i(5) as i32;
        let len = r.i(6) as usize;

        let mut sections = design(kind, order, fn_hz, fc, ripple, subtype);
        let mut noise = common::Noise::new(20261017);
        let mut buffer: Vec<f32> = (0..len).map(|_| noise.next_sample()).collect();
        let mut scratch = vec![0.0f32; len];
        for section in sections.iter_mut() {
            section.process(&buffer, &mut scratch);
            std::mem::swap(&mut buffer, &mut scratch);
        }
        for (i, got) in buffer.iter().enumerate() {
            common::close(
                f64::from(*got),
                r.f(7 + i),
                OUT_REL,
                OUT_ABS,
                &format!(
                    "process-design {} order={} fn={} fc={} ripple={} sub={} sample={}",
                    kind, order, fn_hz, fc, ripple, subtype, i
                ),
            );
        }
        checked += 1;
    }
    assert!(
        checked >= 5,
        "only {} process-design records checked",
        checked
    );
}
