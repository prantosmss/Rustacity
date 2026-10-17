//! C ABI of the DSP core.
//!
//! The declarations live in `include/rustacity_dsp.h`; every function
//! here mirrors one C++ entry point of the original implementation.
//! Pointers are validated before use and no panic may cross the FFI
//! boundary: each entry point runs under [`guard`], which turns a panic
//! into a harmless return value instead of unwinding into C++.
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use core::ffi::c_int;
use std::panic::{catch_unwind, AssertUnwindSafe};

use crate::bass_treble::BassTreble;
use crate::biquad::{self, Biquad};
use crate::ebur128::{self, Ebur128};

/// `rustacity_biquad_t` is 9 doubles - the C++ side asserts that the
/// C++ `Biquad` has exactly the same size.
const _: () =
    assert!(core::mem::size_of::<Biquad>() == 9 * core::mem::size_of::<f64>());

/// Run `f`, returning `fallback` if it panics.
///
/// Every entry point below is wrapped in this guard, because the C++
/// caller cannot cope with an unwind and aborting the process would
/// cost the user whatever they had not saved.  The default panic hook
/// still reports where the panic happened, so the fault stays
/// diagnosable.
///
/// `AssertUnwindSafe` is needed because the closures capture the raw
/// pointers that came from C++, which are not `UnwindSafe`.  That is
/// sound here: a caught panic cannot leave anything observable to the
/// caller.
fn guard<R>(fallback: R, f: impl FnOnce() -> R) -> R {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(value) => value,
        Err(_) => fallback,
    }
}

/// Copy `sections` into the caller's buffer, at most `cap` entries,
/// and return how many were written.
fn write_sections(out: *mut Biquad, cap: usize, sections: Vec<Biquad>) -> c_int {
    if out.is_null() {
        return 0;
    }
    let n = sections.len().min(cap);
    for (i, section) in sections.into_iter().take(n).enumerate() {
        // SAFETY: `out` is non-null and has room for `n <= cap` entries
        // by contract of the caller.
        unsafe { out.add(i).write(section) };
    }
    n as c_int
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_reset(bq: *mut Biquad) {
    guard((), || {
        if bq.is_null() {
            return;
        }
        // SAFETY: non-null, aligned and dereferenceable by contract.
        unsafe { (*bq).reset() };
    });
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_process(
    bq: *mut Biquad,
    input: *const f32,
    output: *mut f32,
    num_samples: c_int,
) {
    guard((), || {
        if bq.is_null() || input.is_null() || output.is_null() || num_samples <= 0 {
            return;
        }
        // SAFETY: the caller guarantees valid pointers for `num_samples`
        // samples; input and output may alias (the loop reads each sample
        // before writing it), so raw pointers are used throughout.
        let bq = unsafe { &mut *bq };
        for i in 0..num_samples as isize {
            let x = unsafe { *input.offset(i) };
            let y = bq.process_one(x);
            unsafe { *output.offset(i) = y };
        }
    });
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_design_butterworth(
    out: *mut Biquad,
    cap: usize,
    order: c_int,
    fn_hz: f64,
    fc: f64,
    subtype: c_int,
) -> c_int {
    guard(0, || {
        write_sections(
            out,
            cap,
            biquad::calc_butterworth_filter(order, fn_hz, fc, subtype),
        )
    })
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_design_chebyshev1(
    out: *mut Biquad,
    cap: usize,
    order: c_int,
    fn_hz: f64,
    fc: f64,
    ripple: f64,
    subtype: c_int,
) -> c_int {
    guard(0, || {
        write_sections(
            out,
            cap,
            biquad::calc_chebyshev_type1_filter(order, fn_hz, fc, ripple, subtype),
        )
    })
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_design_chebyshev2(
    out: *mut Biquad,
    cap: usize,
    order: c_int,
    fn_hz: f64,
    fc: f64,
    ripple: f64,
    subtype: c_int,
) -> c_int {
    guard(0, || {
        write_sections(
            out,
            cap,
            biquad::calc_chebyshev_type2_filter(order, fn_hz, fc, ripple, subtype),
        )
    })
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_cheby_poly(order: c_int, norm_freq: f64) -> f64 {
    guard(0.0, || {
        if !(biquad::MIN_ORDER..=biquad::MAX_ORDER).contains(&order) {
            return 0.0;
        }
        biquad::cheby_poly(order, norm_freq)
    })
}

/// Create the shelf filters of EffectBassTreble for one channel, or null if
/// the arguments are unusable.
#[no_mangle]
pub extern "C" fn rustacity_bass_treble_new(sample_rate: f32) -> *mut BassTreble {
    guard(core::ptr::null_mut(), || {
        if !sample_rate.is_finite() || sample_rate <= 0.0 {
            return core::ptr::null_mut();
        }
        Box::into_raw(Box::new(BassTreble::new(sample_rate)))
    })
}

#[no_mangle]
pub extern "C" fn rustacity_bass_treble_free(handle: *mut BassTreble) {
    guard((), || {
        if handle.is_null() {
            return;
        }
        // SAFETY: the pointer came from `rustacity_bass_treble_new` and is
        // consumed exactly once.
        unsafe { drop(Box::from_raw(handle)) };
    });
}

/// Filter `len` samples of one channel from `input` to `output`, which may
/// alias, with the effect's three parameters for this block; returns `len`,
/// or 0 if the arguments are unusable.
#[no_mangle]
pub extern "C" fn rustacity_bass_treble_process(
    handle: *mut BassTreble,
    bass_db: f64,
    treble_db: f64,
    gain_db: f64,
    input: *const f32,
    output: *mut f32,
    len: usize,
) -> usize {
    guard(0, || {
        if handle.is_null() || input.is_null() || output.is_null() {
            return 0;
        }
        // SAFETY: non-null, aligned and dereferenceable by contract; input
        // and output may alias (each sample is read before it is written),
        // so raw pointers are used throughout.
        let handle = unsafe { &mut *handle };
        handle.set_parameters(bass_db, treble_db, gain_db);
        for i in 0..len {
            let x = unsafe { *input.add(i) };
            let y = handle.process_one(x);
            unsafe { *output.add(i) = y };
        }
        len
    })
}

/// Create a loudness meter, or null if the arguments are unusable.
#[no_mangle]
pub extern "C" fn rustacity_ebur128_new(rate: f64, channels: usize) -> *mut Ebur128 {
    guard(core::ptr::null_mut(), || {
        if !rate.is_finite() || rate <= 0.0 || channels == 0 {
            return core::ptr::null_mut();
        }
        Box::into_raw(Box::new(Ebur128::new(rate, channels)))
    })
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_free(meter: *mut Ebur128) {
    guard((), || {
        if meter.is_null() {
            return;
        }
        // SAFETY: the pointer came from `rustacity_ebur128_new` and is
        // consumed exactly once.
        unsafe { drop(Box::from_raw(meter)) };
    });
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_initialize(meter: *mut Ebur128) {
    guard((), || {
        // SAFETY: `meter.as_mut()` is null-checked.
        if let Some(meter) = unsafe { meter.as_mut() } {
            meter.initialize();
        }
    });
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_process_sample(meter: *mut Ebur128, x: f32, channel: usize) {
    guard((), || {
        // SAFETY: `meter.as_mut()` is null-checked.
        if let Some(meter) = unsafe { meter.as_mut() } {
            meter.process_sample_from_channel(x, channel);
        }
    });
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_next_sample(meter: *mut Ebur128) {
    guard((), || {
        // SAFETY: `meter.as_mut()` is null-checked.
        if let Some(meter) = unsafe { meter.as_mut() } {
            meter.next_sample();
        }
    });
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_process_block(
    meter: *mut Ebur128,
    channels: *const *const f32,
    channel_count: usize,
    len: usize,
) {
    guard((), || {
        if channels.is_null() {
            return;
        }
        // SAFETY: `meter.as_mut()` is null-checked.
        let Some(meter) = (unsafe { meter.as_mut() }) else {
            return;
        };
        let count = channel_count.min(meter.channel_count());
        let mut slices: Vec<&[f32]> = Vec::with_capacity(count);
        for i in 0..count {
            // SAFETY: the caller guarantees `channel_count` valid pointers.
            let channel = unsafe { *channels.add(i) };
            if channel.is_null() {
                return;
            }
            // SAFETY: the caller guarantees `len` readable samples.
            slices.push(unsafe { core::slice::from_raw_parts(channel, len) });
        }
        meter.process_block(&slices);
    });
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_integrative_loudness(meter: *mut Ebur128) -> f64 {
    guard(0.0, || {
        // SAFETY: `meter.as_mut()` is null-checked.
        match unsafe { meter.as_mut() } {
            Some(meter) => meter.integrative_loudness(),
            None => 0.0,
        }
    })
}

/// Fill `out[0..2]` with the K-weighting filter pair (HSF, HPF).
#[no_mangle]
pub extern "C" fn rustacity_ebur128_weighting_filter(
    fs: f64,
    out: *mut Biquad,
    cap: usize,
) -> c_int {
    guard(0, || {
        write_sections(out, cap, ebur128::calc_weighting_filter(fs).to_vec())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The whole point of the guard: a panic inside the core comes back
    /// as a value, never as an unwind into C++ and never as an abort.
    /// The panic message printed by the default hook is expected here.
    #[test]
    fn guard_turns_a_panic_into_the_fallback() {
        assert_eq!(guard(7, || panic!("deliberate test panic")), 7);
    }

    #[test]
    fn guard_passes_a_normal_result_through() {
        assert_eq!(guard(7, || 9), 9);
    }

    /// The nulls and invalid values a C++ caller can produce must be
    /// tolerated, not turned into a process abort.
    #[test]
    fn entry_points_tolerate_bad_arguments() {
        rustacity_biquad_reset(core::ptr::null_mut());
        rustacity_biquad_process(
            core::ptr::null_mut(),
            core::ptr::null(),
            core::ptr::null_mut(),
            16,
        );
        assert_eq!(
            rustacity_biquad_design_butterworth(core::ptr::null_mut(), 0, 3, 48000.0, 1000.0, 0),
            0
        );
        assert_eq!(rustacity_biquad_cheby_poly(0, 0.5), 0.0);
        assert_eq!(rustacity_biquad_cheby_poly(99, 0.5), 0.0);

        assert!(rustacity_ebur128_new(f64::NAN, 2).is_null());
        assert!(rustacity_ebur128_new(48000.0, 0).is_null());

        assert!(rustacity_bass_treble_new(0.0).is_null());
        assert!(rustacity_bass_treble_new(f32::NAN).is_null());
        rustacity_bass_treble_free(core::ptr::null_mut());
        assert_eq!(
            rustacity_bass_treble_process(
                core::ptr::null_mut(),
                0.0,
                0.0,
                0.0,
                core::ptr::null(),
                core::ptr::null_mut(),
                8
            ),
            0
        );
        rustacity_ebur128_initialize(core::ptr::null_mut());
        rustacity_ebur128_process_sample(core::ptr::null_mut(), 0.0, 0);
        rustacity_ebur128_next_sample(core::ptr::null_mut());
        rustacity_ebur128_process_block(core::ptr::null_mut(), core::ptr::null(), 0, 0);
        assert_eq!(
            rustacity_ebur128_integrative_loudness(core::ptr::null_mut()),
            0.0
        );
        rustacity_ebur128_free(core::ptr::null_mut());
        assert_eq!(
            rustacity_ebur128_weighting_filter(48000.0, core::ptr::null_mut(), 0),
            0
        );
    }
}
