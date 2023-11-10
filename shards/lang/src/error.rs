use pest::RuleType;
use std::fmt::Display;

pub type Error = Box<dyn std::error::Error>;

pub fn fmt_err<M: Display>(msg: M, r: &pest::Span) -> Error {
  format!("{}: {}", r.as_str(), msg).to_string().into()
}

pub fn fmt_errp<'i, M: Display, R: RuleType>(msg: M, pair: &pest::iterators::Pair<'i, R>) -> Error {
  fmt_err(msg, &pair.as_span())
}
