//! EBU R128 loudness measurement.
//!
//! Faithful port of the original C++ implementation
//! (`src/effects/EBUR128.{h,cpp}`): 400 ms measurement blocks with a
//! 100 ms step, a 65536 bin loudness histogram and the K-weighting
//! filter pair designed by `calc_weighting_filter` (sampling rate
//! adaption after Mansbridge, Finn and Reiss, AES 132nd Convention,
//! 2012).
//!
//! Contributor: Sadik Pranto <pranto.smss.dev@gmail.com>

use std::f64::consts::PI;

use crate::biquad::Biquad;

/// Number of histogram bins (`EBUR128::HIST_BIN_COUNT`).
pub const HIST_BIN_COUNT: usize = 65536;
/// EBU R128 absolute threshold (`EBUR128::GAMMA_A`).
pub const GAMMA_A: f64 = (-70.0 + 0.691) / 10.0;

/// The rounded histogram index for `value`, or `None` when the C++
/// code would end up outside the histogram.
///
/// The C++ implementation converts the rounded value to `size_t` and
/// only ever uses it after a `< HIST_BIN_COUNT` check, so negative
/// values and NaN (which convert to huge values) are skipped.
fn histogram_index(value: f64) -> Option<usize> {
    let rounded = value.round();
    if !rounded.is_finite() || rounded < 0.0 || rounded >= HIST_BIN_COUNT as f64 {
        return None;
    }
    Some(rounded as usize)
}

/// EBU R128 loudness meter.
///
/// Mirrors the C++ class of the same name; the C ABI used by the
/// application is declared in `include/rustacity_dsp.h`
/// (`rustacity_ebur128_t`).
pub struct Ebur128 {
    loudness_hist: Vec<i64>,
    block_ring_buffer: Vec<f64>,
    sample_count: usize,
    block_ring_pos: usize,
    block_ring_size: usize,
    block_size: usize,
    block_overlap: usize,
    channel_count: usize,
    rate: f64,
    /// `weighting_filter[channel][filter]` with `filter` HSF = 0,
    /// HPF = 1.
    weighting_filter: Vec<[Biquad; 2]>,
}

impl Ebur128 {
    /// Create a meter for `rate` samples per second and `channels`
    /// channels, mirroring the C++ constructor.
    pub fn new(rate: f64, channels: usize) -> Self {
        let block_size = (0.4 * rate).ceil() as usize; // 400 ms blocks
        let block_overlap = (0.1 * rate).ceil() as usize; // 100 ms overlap
        let mut meter = Ebur128 {
            loudness_hist: vec![0; HIST_BIN_COUNT],
            block_ring_buffer: vec![0.0; block_size],
            sample_count: 0,
            block_ring_pos: 0,
            block_ring_size: 0,
            block_size,
            block_overlap,
            channel_count: channels,
            rate,
            weighting_filter: Vec::with_capacity(channels),
        };
        for _ in 0..channels {
            meter.weighting_filter.push(calc_weighting_filter(rate));
        }
        meter
    }

    /// Number of channels this meter was created for.
    pub fn channel_count(&self) -> usize {
        self.channel_count
    }

    /// Sample rate this meter was created for.
    pub fn rate(&self) -> f64 {
        self.rate
    }

    /// Reset all measurement state, mirroring `EBUR128::Initialize`.
    pub fn initialize(&mut self) {
        self.sample_count = 0;
        self.block_ring_pos = 0;
        self.block_ring_size = 0;
        self.loudness_hist.iter_mut().for_each(|bin| *bin = 0);
        for filters in self.weighting_filter.iter_mut() {
            filters[0].reset();
            filters[1].reset();
        }
    }

    /// Filter one input sample of `channel` and accumulate its power,
    /// mirroring `EBUR128::ProcessSampleFromChannel`.
    ///
    /// Out-of-range channels are ignored (the C++ code would write out
    /// of bounds).
    pub fn process_sample_from_channel(&mut self, x_in: f32, channel: usize) {
        if channel >= self.channel_count {
            return;
        }
        let mut value = self.weighting_filter[channel][0].process_one(x_in);
        value = self.weighting_filter[channel][1].process_one(value);
        let power = f64::from(value) * f64::from(value);
        if channel == 0 {
            self.block_ring_buffer[self.block_ring_pos] = power;
        } else {
            // Add the power of additional channels to the power of the
            // first channel.  As a result, stereo tracks appear about
            // 3 LUFS louder, as specified.
            self.block_ring_buffer[self.block_ring_pos] += power;
        }
    }

    /// Advance to the next sample, mirroring `EBUR128::NextSample`.
    pub fn next_sample(&mut self) {
        self.block_ring_pos += 1;
        self.block_ring_size += 1;

        // The C++ code uses `% mBlockOverlap`, which is only defined
        // for a positive overlap; a sample rate of at least 10 Hz (the
        // smallest sensible value) always yields one.
        if self.block_overlap != 0 && self.block_ring_pos % self.block_overlap == 0 {
            // A new full block of samples was submitted.
            if self.block_ring_size >= self.block_size {
                self.add_block_to_histogram(self.block_size);
            }
        }
        // Close the ring.
        if self.block_ring_pos == self.block_size {
            self.block_ring_pos = 0;
        }
        self.sample_count += 1;
    }

    /// Process a block of interleaved-by-channel input, equivalent to
    /// calling [`Ebur128::process_sample_from_channel`] for every
    /// channel of every sample followed by [`Ebur128::next_sample`].
    ///
    /// `channels[0]` is always processed first, like in the C++ caller.
    pub fn process_block(&mut self, channels: &[&[f32]]) {
        let count = channels.len().min(self.channel_count);
        // The C++ caller always passes equally sized buffers.
        let len = channels[..count].iter().map(|c| c.len()).min().unwrap_or(0);
        for i in 0..len {
            for (channel, data) in channels[..count].iter().enumerate() {
                self.process_sample_from_channel(data[i], channel);
            }
            self.next_sample();
        }
    }

    /// Gated loudness of all processed samples, mirroring
    /// `EBUR128::IntegrativeLoudness`.
    pub fn integrative_loudness(&mut self) -> f64 {
        // EBU R128: z_i = mean square without root

        // Calculate Gamma_R from histogram.
        let (mut sum_v, mut sum_c) = self.histogram_sums(0);

        // Handle incomplete block if no non-zero block was found.
        if sum_c == 0 {
            let block_ring_size = self.block_ring_size;
            self.add_block_to_histogram(block_ring_size);
            let res = self.histogram_sums(0);
            sum_v = res.0;
            sum_c = res.1;
        }

        // Histogram values are simplified log(x^2) immediate values
        // without -0.691 + 10*(...) to safe computing power. This is
        // possible because they will cancel out anyway.
        // The -1 in the line below is the -10 LUFS from the EBU R128
        // specification without the scaling factor of 10.
        let gamma_r = (sum_v / sum_c as f64).log10() - 1.0;
        // Out-of-range indices (NaN for silence, huge for extreme
        // level) skip every bin, so `sum_c` stays zero below.
        let idx_r = histogram_index((gamma_r - GAMMA_A) * (HIST_BIN_COUNT as f64) / -GAMMA_A - 1.0);

        // Apply Gamma_R threshold and calculate gated loudness (extent).
        let start_idx = match idx_r {
            Some(idx) => idx.saturating_add(1),
            None => HIST_BIN_COUNT,
        };
        let (sum_v, sum_c) = self.histogram_sums(start_idx);
        if sum_c == 0 {
            // Silence was processed.
            return 0.0;
        }
        // LUFS is defined as -0.691 dB + 10*log10(sum(channels))
        0.8529037031 * sum_v / sum_c as f64
    }

    /// Gated loudness in LUFS, mirroring
    /// `EBUR128::IntegrativeLoudnessToLUFS`.
    pub fn integrative_loudness_to_lufs(&self, loudness: f64) -> f64 {
        10.0 * loudness.log10()
    }

    /// Weighted power and block count of all histogram bins from
    /// `start_idx` on, mirroring `EBUR128::HistogramSums`.
    fn histogram_sums(&self, start_idx: usize) -> (f64, i64) {
        let mut sum_v = 0.0;
        let mut sum_c: i64 = 0;
        for i in start_idx..HIST_BIN_COUNT {
            let val = -GAMMA_A / (HIST_BIN_COUNT as f64) * ((i + 1) as f64) + GAMMA_A;
            sum_v += 10f64.powf(val) * self.loudness_hist[i] as f64;
            sum_c += self.loudness_hist[i];
        }
        (sum_v, sum_c)
    }

    /// Add the current block to the histogram, mirroring
    /// `EBUR128::AddBlockToHistogram`.
    ///
    /// `valid_len` is only smaller than the block size when the audio
    /// to be processed is shorter than one full block.
    fn add_block_to_histogram(&mut self, valid_len: usize) {
        // Reset the ring size to the full state to avoid overflow.
        // The actual value does not matter since it is only used to
        // detect if blocks are complete (>= block size).
        self.block_ring_size = self.block_size;

        let mut block_val = 0.0;
        for i in 0..valid_len.min(self.block_ring_buffer.len()) {
            block_val += self.block_ring_buffer[i];
        }

        // Histogram values are simplified log10() immediate values
        // without -0.691 + 10*(...) to safe computing power. This is
        // possible because these constants cancel out anyway during
        // the following processing steps.
        block_val = (block_val / valid_len as f64).log10();
        // log(blockVal) is within ]-inf, 1]
        if let Some(idx) =
            histogram_index((block_val - GAMMA_A) * (HIST_BIN_COUNT as f64) / -GAMMA_A - 1.0)
        {
            // idx is within ]-inf, HIST_BIN_COUNT-1], discard indices
            // below 0 as they are below the EBU R128 absolute
            // threshold anyway.
            self.loudness_hist[idx] += 1;
        }
    }
}

/// EBU R128 weighting filter pair (HSF and HPF), mirroring
/// `EBUR128::CalcWeightingFilter`.
///
/// Sampling rate adaption after Mansbridge, Stuart, Saoirse Finn, and
/// Joshua D. Reiss. "Implementation and Evaluation of Autonomous
/// Multi-track Fader Control." Paper presented at the 132nd Audio
/// Engineering Society Convention, Budapest, Hungary, 2012.
pub fn calc_weighting_filter(fs: f64) -> [Biquad; 2] {
    let mut p_biquad = [Biquad::new(); 2];

    //
    // HSF pre filter
    //
    let db = 3.999843853973347;
    let f0 = 1681.974450955533;
    let q = 0.7071752369554196;
    let mut k = (PI * f0 / fs).tan();

    let vh = 10f64.powf(db / 20.0);
    let vb = vh.powf(0.4996667741545416);

    let a0 = 1.0 + k / q + k * k;

    p_biquad[0].numer[0] = (vh + vb * k / q + k * k) / a0;
    p_biquad[0].numer[1] = 2.0 * (k * k - vh) / a0;
    p_biquad[0].numer[2] = (vh - vb * k / q + k * k) / a0;

    p_biquad[0].denom[0] = 2.0 * (k * k - 1.0) / a0;
    p_biquad[0].denom[1] = (1.0 - k / q + k * k) / a0;

    //
    // HPF weighting filter
    //
    let f0 = 38.13547087602444;
    let q = 0.5003270373238773;
    k = (PI * f0 / fs).tan();

    p_biquad[1].numer[0] = 1.0;
    p_biquad[1].numer[1] = -2.0;
    p_biquad[1].numer[2] = 1.0;

    p_biquad[1].denom[0] = 2.0 * (k * k - 1.0) / (1.0 + k / q + k * k);
    p_biquad[1].denom[1] = (1.0 - k / q + k * k) / (1.0 + k / q + k * k);

    p_biquad
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn silence_has_no_loudness() {
        let mut meter = Ebur128::new(48000.0, 2);
        meter.initialize();
        for _ in 0..48000 {
            meter.process_sample_from_channel(0.0, 0);
            meter.process_sample_from_channel(0.0, 1);
            meter.next_sample();
        }
        assert_eq!(meter.integrative_loudness(), 0.0);
    }

    #[test]
    fn shorter_than_one_block_is_measured() {
        let mut meter = Ebur128::new(48000.0, 1);
        meter.initialize();
        for _ in 0..1000 {
            meter.process_sample_from_channel(0.25, 0);
            meter.next_sample();
        }
        let loudness = meter.integrative_loudness();
        assert!(loudness > 0.0 && loudness.is_finite(), "loudness {}", loudness);
    }

    #[test]
    fn block_processing_matches_sample_processing() {
        let rate = 48000.0;
        let channels = 2;
        let len = 5000;
        let noise: Vec<f32> = (0..len)
            .map(|i| ((i as f64 / 7.0).sin() * 0.5) as f32)
            .collect();

        let mut per_sample = Ebur128::new(rate, channels);
        per_sample.initialize();
        for i in 0..len {
            per_sample.process_sample_from_channel(noise[i], 0);
            per_sample.process_sample_from_channel(noise[i], 1);
            per_sample.next_sample();
        }

        let mut block = Ebur128::new(rate, channels);
        block.initialize();
        block.process_block(&[&noise, &noise]);

        let expected = per_sample.integrative_loudness();
        let got = block.integrative_loudness();
        assert_eq!(expected, got);
    }

    #[test]
    fn weighting_filter_is_stable_and_normalized() {
        let filters = calc_weighting_filter(48000.0);
        // The HPF section has the canonical -2 / (K^2 - 1) shape.
        assert_eq!(filters[1].numer[0], 1.0);
        assert_eq!(filters[1].numer[1], -2.0);
        assert_eq!(filters[1].numer[2], 1.0);
        for filter in &filters {
            let a1 = filter.denom[0];
            let a2 = filter.denom[1];
            assert!(a1.is_finite() && a2.is_finite());
            // Both poles have to be inside the unit circle.
            assert!(
                a2.abs() < 1.0 && a1.abs() < 1.0 + a2,
                "unstable section: a1={} a2={}",
                a1,
                a2
            );
        }
    }
}
