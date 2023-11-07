use xxhash_rust::xxh3::xxh3_64;
pub struct Span {
  start: usize,
  end: usize,
  file_hash: u64,
}

pub fn hash_file_name(path: &str) -> u64 {
  xxh3_64(path.as_bytes())
}
