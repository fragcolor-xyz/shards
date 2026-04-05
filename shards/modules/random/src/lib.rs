#[macro_use]
extern crate lazy_static;

use shards::{
  core::{register_shard, Core},
  cstr,
  shard::Shard,
  shccstr,
  types::{ClonedVar, Context, OptionalString, Types, Var, INT_TYPES, NONE_TYPES, STRING_TYPES},
  SHCore,
};

use rand::seq::IndexedRandom;

const ADJECTIVES: &[&str] = &[
  "able", "acid", "angry", "apt", "aware", "back", "bad", "bare", "basic", "best",
  "big", "bold", "brave", "brief", "broad", "brown", "busy", "calm", "cheap", "chief",
  "civil", "clean", "clear", "close", "cold", "cool", "crude", "cute", "dark", "dear",
  "deep", "dense", "dirty", "dry", "dual", "dull", "dumb", "eager", "early", "easy",
  "equal", "even", "evil", "exact", "extra", "faint", "fair", "false", "fancy", "far",
  "fast", "fat", "few", "final", "fine", "firm", "first", "fit", "flat", "fond",
  "free", "fresh", "full", "fun", "giant", "glad", "good", "grand", "grave", "gray",
  "great", "green", "gross", "happy", "hard", "harsh", "heavy", "high", "holy", "hot",
  "huge", "human", "humble", "ideal", "ill", "inner", "keen", "key", "kind", "known",
  "large", "last", "late", "lazy", "left", "legal", "light", "live", "local", "long",
];

const NOUNS: &[&str] = &[
  "acid", "age", "air", "angle", "ant", "apple", "arc", "arm", "army", "art",
  "atom", "award", "baby", "back", "badge", "bag", "ball", "band", "bank", "bar",
  "base", "basin", "bat", "bath", "bear", "beat", "bed", "bell", "bench", "berry",
  "bird", "blade", "block", "board", "boat", "body", "bolt", "bomb", "bone", "book",
  "booth", "bow", "box", "brain", "brand", "bread", "brick", "bridge", "brush", "buddy",
  "bug", "bulk", "bus", "buyer", "cabin", "cake", "camp", "cap", "car", "card",
  "cargo", "case", "cash", "cast", "cat", "chain", "chair", "chart", "cheek", "chest",
  "child", "chip", "chunk", "city", "claim", "clan", "cliff", "clock", "cloud", "coach",
  "coast", "code", "coin", "color", "colt", "comet", "coral", "core", "court", "craft",
  "crane", "crash", "cream", "crew", "cross", "crowd", "crown", "crush", "curve", "cycle",
];

#[derive(shards::shard)]
#[shard_info("Random.Name", "Generate a random name (Petname)")]
pub struct RandomName {
  #[shard_param("Words", "How many words to generate and concatenate", INT_TYPES)]
  pub words_count: ClonedVar,
  #[shard_param(
    "Separator",
    "A separator character to use between generated words",
    STRING_TYPES
  )]
  pub separator: ClonedVar,
  output: ClonedVar,
}

impl Default for RandomName {
  fn default() -> Self {
    Self {
      words_count: 2.into(),
      separator: Var::ephemeral_string("-").into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for RandomName {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Option<Var>, &str> {
    let words_count: i64 = self.words_count.0.as_ref().try_into()?;
    let separator: &str = self.separator.0.as_ref().try_into()?;
    let mut rng = rand::rng();
    let words: Vec<&str> = (0..words_count as usize)
      .map(|i| {
        if i % 2 == 0 {
          *ADJECTIVES.choose(&mut rng).unwrap_or(&"unknown")
        } else {
          *NOUNS.choose(&mut rng).unwrap_or(&"thing")
        }
      })
      .collect();
    let pname = words.join(separator);
    self.output = pname.into();
    Ok(Some(self.output.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_random_rust(core: *mut SHCore) {
  unsafe {
    Core = core;
  }
  register_shard::<RandomName>();
}
