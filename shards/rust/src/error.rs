use std::{
  error::Error,
  fmt::{self, Debug, Display},
};

pub enum FastError {
  Static(&'static str),
  Dynamic(String),
}

impl From<String> for FastError {
  fn from(s: String) -> Self {
    FastError::Dynamic(s)
  }
}

impl From<&'static str> for FastError {
  fn from(s: &'static str) -> Self {
    FastError::Static(s)
  }
}

impl FastError {
  pub fn str(&self) -> &str {
    match self {
      FastError::Static(s) => s,
      FastError::Dynamic(s) => s.as_str(),
    }
  }
}

impl Into<String> for FastError {
  fn into(self) -> String {
    match self {
      FastError::Static(s) => s.to_string(),
      FastError::Dynamic(s) => s,
    }
  }
}

impl Into<String> for &FastError {
  fn into(self) -> String {
    match self {
      FastError::Static(s) => s.to_string(),
      FastError::Dynamic(s) => s.to_string(),
    }
  }
}

impl Error for FastError {}

impl Display for FastError {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    match self {
      FastError::Static(s) => write!(f, "{}", s),
      FastError::Dynamic(s) => write!(f, "{}", s),
    }
  }
}

impl Debug for FastError {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    match self {
      FastError::Static(s) => write!(f, "{}", s),
      FastError::Dynamic(s) => write!(f, "{}", s),
    }
  }
}
