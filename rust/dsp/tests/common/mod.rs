//! Shared helpers for the golden-value tests.
//!
//! The records come from `rust/dsp/goldens/reference.txt`, captured
//! from the ORIGINAL C++ implementation by `src/goldendump.cpp`.

/// One `GOLDEN`-style record: the record kind plus its fields.
pub struct Record {
    pub kind: String,
    pub fields: Vec<String>,
}

/// Load all records from the checked-in golden file.
pub fn load() -> Vec<Record> {
    include_str!("../../goldens/reference.txt")
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .map(|line| {
            let mut tokens = line.split_whitespace();
            let kind = tokens.next().unwrap().to_owned();
            Record {
                kind,
                fields: tokens.map(str::to_owned).collect(),
            }
        })
        .collect()
}

impl Record {
    pub fn s(&self, index: usize) -> &str {
        &self.fields[index]
    }

    pub fn i(&self, index: usize) -> i64 {
        self.fields[index].parse().expect("integer field")
    }

    pub fn f(&self, index: usize) -> f64 {
        self.fields[index].parse().expect("float field")
    }
}

/// Assert that `got` is within `max(abs_tol, rel_tol * |want|)`.
pub fn close(got: f64, want: f64, rel_tol: f64, abs_tol: f64, context: &str) {
    let diff = (got - want).abs();
    let tolerance = abs_tol.max(rel_tol * want.abs());
    assert!(
        diff <= tolerance,
        "{}: got {:.17e}, want {:.17e} (diff {:.3e} > tol {:.3e})",
        context,
        got,
        want,
        diff,
        tolerance
    );
}

/// Deterministic noise generator, mirroring `NextNoise` in
/// `src/goldendump.cpp` (wrapping u32 LCG; the sample is
/// `state / 2^32 - 1` rounded to float).
pub struct Noise(u32);

impl Noise {
    pub fn new(seed: u32) -> Self {
        Noise(seed)
    }

    pub fn next_sample(&mut self) -> f32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        ((f64::from(self.0) / 4294967296.0) - 1.0) as f32
    }
}
