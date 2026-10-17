//! C ABI of the DSP core.
//!
//! The declarations live in `include/rustacity_dsp.h`; every function
//! here mirrors one C++ entry point of the original implementation.
//! Pointers are validated before use and no panic can cross the FFI
//! boundary (the release profile aborts instead of unwinding).
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use core::ffi::c_int;

use crate::biquad::{self, Biquad};
use crate::ebur128::{self, Ebur128};

/// `rustacity_biquad_t` is 9 doubles - the C++ side asserts that the
/// C++ `Biquad` has exactly the same size.
const _: () =
    assert!(core::mem::size_of::<Biquad>() == 9 * core::mem::size_of::<f64>());

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
    if bq.is_null() {
        return;
    }
    // SAFETY: non-null, aligned and dereferenceable by contract.
    unsafe { (*bq).reset() };
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_process(
    bq: *mut Biquad,
    input: *const f32,
    output: *mut f32,
    num_samples: c_int,
) {
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
    write_sections(
        out,
        cap,
        biquad::calc_butterworth_filter(order, fn_hz, fc, subtype),
    )
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
    write_sections(
        out,
        cap,
        biquad::calc_chebyshev_type1_filter(order, fn_hz, fc, ripple, subtype),
    )
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
    write_sections(
        out,
        cap,
        biquad::calc_chebyshev_type2_filter(order, fn_hz, fc, ripple, subtype),
    )
}

#[no_mangle]
pub extern "C" fn rustacity_biquad_cheby_poly(order: c_int, norm_freq: f64) -> f64 {
    if !(biquad::MIN_ORDER..=biquad::MAX_ORDER).contains(&order) {
        return 0.0;
    }
    biquad::cheby_poly(order, norm_freq)
}

/// Create a loudness meter, or null if the arguments are unusable.
#[no_mangle]
pub extern "C" fn rustacity_ebur128_new(rate: f64, channels: usize) -> *mut Ebur128 {
    if !rate.is_finite() || rate <= 0.0 || channels == 0 {
        return core::ptr::null_mut();
    }
    Box::into_raw(Box::new(Ebur128::new(rate, channels)))
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_free(meter: *mut Ebur128) {
    if meter.is_null() {
        return;
    }
    // SAFETY: the pointer came from `rustacity_ebur128_new` and is
    // consumed exactly once.
    unsafe { drop(Box::from_raw(meter)) };
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_initialize(meter: *mut Ebur128) {
    // SAFETY: `meter.as_mut()` is null-checked.
    if let Some(meter) = unsafe { meter.as_mut() } {
        meter.initialize();
    }
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_process_sample(meter: *mut Ebur128, x: f32, channel: usize) {
    // SAFETY: `meter.as_mut()` is null-checked.
    if let Some(meter) = unsafe { meter.as_mut() } {
        meter.process_sample_from_channel(x, channel);
    }
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_next_sample(meter: *mut Ebur128) {
    // SAFETY: `meter.as_mut()` is null-checked.
    if let Some(meter) = unsafe { meter.as_mut() } {
        meter.next_sample();
    }
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_process_block(
    meter: *mut Ebur128,
    channels: *const *const f32,
    channel_count: usize,
    len: usize,
) {
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
}

#[no_mangle]
pub extern "C" fn rustacity_ebur128_integrative_loudness(meter: *mut Ebur128) -> f64 {
    // SAFETY: `meter.as_mut()` is null-checked.
    match unsafe { meter.as_mut() } {
        Some(meter) => meter.integrative_loudness(),
        None => 0.0,
    }
}

/// Fill `out[0..2]` with the K-weighting filter pair (HSF, HPF).
#[no_mangle]
pub extern "C" fn rustacity_ebur128_weighting_filter(
    fs: f64,
    out: *mut Biquad,
    cap: usize,
) -> c_int {
    write_sections(out, cap, ebur128::calc_weighting_filter(fs).to_vec())
}

