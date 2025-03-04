/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

use hmac::Hmac;
use pbkdf2::pbkdf2;
use serde_json::Value;
use sha2::Sha512;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::SeqVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_TYPES;
use shards::types::BYTES_TYPES;
use shards::types::SEQ_OF_STRINGS_OR_SEQ_OF_BYTES_TYPES;
use shards::types::SEQ_OF_STRING_OR_BYTE_TYPES;
use shards::types::STRING_TYPES;
use std::convert::TryInto;

#[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
use {
  jsonwebtoken::jwk::Jwk,
  jsonwebtoken::jwk::KeyAlgorithm,
  jsonwebtoken::{decode, decode_header, Algorithm, DecodingKey, Validation},
};

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

pub mod argon;
pub mod chachapoly;
pub mod ecdsa;
pub mod hash;
pub mod signatures;

static CRYPTO_KEY_TYPES: &[Type] = &[common_type::bytes, common_type::bytes_var];

use bip39::{Language, Mnemonic, MnemonicType};

lazy_static! {
  pub static ref MNEMONIC_INPUT_TYPES: Vec<Type> = vec![common_type::int];
  pub static ref MNEMONIC_OUTPUT_TYPES: Vec<Type> = vec![common_type::string];
}

#[derive(shards::shard)]
#[shard_info("Mnemonic.Generate", "Generates a BIP39 mnemonic")]
struct MnemonicGenerate {
  output: ClonedVar,
}

impl Default for MnemonicGenerate {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for MnemonicGenerate {
  fn input_types(&mut self) -> &Types {
    &MNEMONIC_INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &MNEMONIC_OUTPUT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let size: i64 = input.try_into().unwrap();
    let mnemonic_type = match size {
      12 => MnemonicType::Words12,
      15 => MnemonicType::Words15,
      18 => MnemonicType::Words18,
      21 => MnemonicType::Words21,
      24 => MnemonicType::Words24,
      _ => return Err("Invalid mnemonic size"),
    };
    let mnemonic = Mnemonic::new(mnemonic_type, Language::English);
    self.output = Var::ephemeral_string(mnemonic.phrase()).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Mnemonic.ToSeed", "Converts a BIP39 mnemonic to a seed")]
struct MnemonicToSeed {
  output: ClonedVar,
}

impl Default for MnemonicToSeed {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

impl MnemonicToSeed {
  // for legacy reasons, we use the way substrate bip39 does it
  pub fn seed_from_entropy(entropy: &[u8], password: &str) -> Result<[u8; 64], &'static str> {
    if entropy.len() < 16 || entropy.len() > 32 || entropy.len() % 4 != 0 {
      return Err("Invalid entropy length");
    }

    let mut salt = String::with_capacity(8 + password.len());
    salt.push_str("mnemonic");
    salt.push_str(password);

    let mut seed = [0u8; 64];

    pbkdf2::<Hmac<Sha512>>(entropy, salt.as_bytes(), 2048, &mut seed)
      .map_err(|_| "PBKDF2 error")?;

    Ok(seed)
  }
}

#[shards::shard_impl]
impl Shard for MnemonicToSeed {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let string: &str = input.try_into().unwrap();
    let mnemonic =
      Mnemonic::from_phrase(string, Language::English).map_err(|_| "Invalid mnemonic")?;
    let seed = Self::seed_from_entropy(mnemonic.entropy(), "").map_err(|_| "Invalid entropy")?;
    // let seed = Seed::new(&mnemonic, "");
    self.output = Var::ephemeral_slice(seed.as_slice()).into();
    Ok(Some(self.output.0))
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[derive(shards::shard)]
#[shard_info("Jwt.Decode", "Decodes a JWT token")]
struct JwtDecode {
  output: ClonedVar,

  #[shard_param("Jwk", "The Key in JWK format to use for decoding the token.", [common_type::string, common_type::string_var])]
  jwk: ParamVar,

  #[shard_param("Audience", "The audience to use for decoding the token.", [common_type::string, common_type::string_var])]
  audience: ParamVar,
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
impl Default for JwtDecode {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      jwk: ParamVar::default(),
      audience: ParamVar::default(),
    }
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[shards::shard_impl]
impl Shard for JwtDecode {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let jwk: &str = self.jwk.get().try_into().unwrap();
    let jwk = serde_json::from_str::<Jwk>(jwk).unwrap();
    let decoding_key = DecodingKey::from_jwk(&jwk).map_err(|e| {
      shlog_error!("Invalid JWK: {}", e);
      "Invalid JWK"
    })?;

    // Set up validation
    let algo = jwk.common.key_algorithm.ok_or("Unsupported key type")?;
    let mut validation = Validation::new(match algo {
      KeyAlgorithm::ES256 => Algorithm::ES256,
      KeyAlgorithm::ES384 => Algorithm::ES384,
      KeyAlgorithm::RS256 => Algorithm::RS256,
      KeyAlgorithm::RS384 => Algorithm::RS384,
      _ => return Err("Unsupported key type"),
    });
    let audience: &str = self.audience.get().try_into().unwrap();
    validation.set_audience(&[audience]);

    // Decode and verify the token
    let token: &str = input.try_into().unwrap();
    let token_data = decode::<Value>(token, &decoding_key, &validation).map_err(|e| {
      shlog_error!("Invalid token: {}", e);
      "Invalid token"
    })?;

    let json_string = serde_json::to_string(&token_data.claims).map_err(|e| {
      shlog_error!("Failed to convert token data to JSON: {}", e);
      "Failed to convert token data to JSON"
    })?;

    self.output = Var::ephemeral_string(&json_string).into();

    Ok(Some(self.output.0))
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[derive(shards::shard)]
#[shard_info(
  "Jwt.Verify",
  "Verifies a JWT token signature without fully decoding it"
)]
struct JwtVerify {
  #[shard_param("Jwk", "The Key in JWK format to use for verifying the token signature.", [common_type::none, common_type::string, common_type::string_var])]
  jwk: ParamVar,
  #[shard_param("PemEc", "The Key in PEM ECDSA format to use for verifying the token signature.", [common_type::none, common_type::string, common_type::string_var])]
  pem_ec: ParamVar,
  #[shard_param("PemRsa", "The Key in PEM RSA format to use for verifying the token signature.", [common_type::none, common_type::string, common_type::string_var])]
  pem_rsa: ParamVar,
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
impl Default for JwtVerify {
  fn default() -> Self {
    Self {
      jwk: ParamVar::default(),
      pem_ec: ParamVar::default(),
      pem_rsa: ParamVar::default(),
    }
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[shards::shard_impl]
impl Shard for JwtVerify {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    use jsonwebtoken::errors::ErrorKind;

    let decoding_key = if !self.jwk.get().is_none() {
      let jwk: &str = self.jwk.get().try_into().unwrap();
      let jwk = serde_json::from_str::<Jwk>(jwk).unwrap();
      DecodingKey::from_jwk(&jwk).map_err(|e| {
        shlog_error!("Invalid JWK: {}", e);
        "Invalid JWK"
      })
    } else if !self.pem_ec.get().is_none() {
      let pem: &str = self.pem_ec.get().try_into().unwrap();
      DecodingKey::from_ec_pem(pem.as_bytes()).map_err(|e| {
        shlog_error!("Invalid PEM ECDSA key: {}", e);
        "Invalid PEM ECDSA key"
      })
    } else if !self.pem_rsa.get().is_none() {
      let pem: &str = self.pem_rsa.get().try_into().unwrap();
      DecodingKey::from_rsa_pem(pem.as_bytes()).map_err(|e| {
        shlog_error!("Invalid PEM RSA key: {}", e);
        "Invalid PEM RSA key"
      })
    } else {
      Err("No key provided")
    }?;

    // Only verify the signature without validating claims
    let token: &str = input.try_into().unwrap();

    let header = decode_header(token).map_err(|e| {
      shlog_error!("Invalid token: {}", e);
      "Invalid token"
    })?;

    // Set up validation
    let validation = Validation::new(header.alg);

    // Use decode with a dummy Claims struct but enable validate_exp=false
    // to only validate the signature
    let mut validation = validation;
    validation.validate_exp = false;
    validation.validate_aud = false;
    validation.required_spec_claims.clear(); // Remove all required claims

    let result = decode::<Value>(token, &decoding_key, &validation);

    // Check if signature validation succeeded
    match result {
      Ok(_) => Ok(Some(true.into())),
      Err(err) => {
        match err.kind() {
          // These errors are related to signature verification
          ErrorKind::InvalidSignature
          | ErrorKind::InvalidAlgorithm
          | ErrorKind::InvalidKeyFormat => {
            shlog_error!("Signature verification failed: {}", err);
            Ok(Some(false.into()))
          }
          // Other errors might be related to token format or parsing
          _ => {
            shlog_error!("JWT parsing error: {}", err);
            Err("Invalid token format")
          }
        }
      }
    }
  }
}

use openssl::x509::X509;

#[derive(shards::shard)]
#[shard_info(
  "X509.Verify",
  "Verifies a certificate chain against a root certificate"
)]
struct X509Verify {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("RootCert", "The X509 root certificate in PEM format to use for verification.", [common_type::none, common_type::bytes, common_type::bytes_var, common_type::string, common_type::string_var])]
  root_cert: ParamVar,
}

impl Default for X509Verify {
  fn default() -> Self {
    Self {
      root_cert: ParamVar::default(),
      required: ExposedTypes::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for X509Verify {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_STRINGS_OR_SEQ_OF_BYTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.root_cert.warmup(ctx);
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.root_cert.cleanup(ctx);
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(common_type::bool)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get the certificate chain from input
    let chain_pems: SeqVar = input.try_into().unwrap();

    let root_cert = if self.root_cert.get().is_string() {
      let root_cert_pem: &str = self.root_cert.get().try_into().unwrap();
      X509::from_pem(root_cert_pem.as_bytes()).map_err(|e| {
        shlog_error!("Failed to parse root certificate: {}", e);
        "Failed to parse root certificate"
      })?
    } else if self.root_cert.get().is_bytes() {
      let root_cert_pem: &[u8] = self.root_cert.get().try_into().unwrap();
      X509::from_der(root_cert_pem).map_err(|e| {
        shlog_error!("Failed to parse root certificate: {}", e);
        "Failed to parse root certificate"
      })?
    } else {
      return Err("Root certificate parameter is required");
    };

    let chain_certs = chain_pems
      .iter()
      .map(|pem| {
        if pem.is_string() {
          let pem_str: &str = pem.as_ref().try_into().unwrap();
          X509::from_pem(pem_str.as_bytes()).map_err(|e| {
            shlog_error!("Failed to parse certificate in chain: {}", e);
            "Failed to parse certificate in chain"
          })
        } else if pem.is_bytes() {
          let pem_bytes: &[u8] = pem.as_ref().try_into().unwrap();
          X509::from_der(pem_bytes).map_err(|e| {
            shlog_error!("Failed to parse certificate in chain: {}", e);
            "Failed to parse certificate in chain"
          })
        } else {
          Err("Certificate must be either string or bytes")
        }
      })
      .collect::<Vec<_>>();

    // make sure last is equal to root_cert
    match chain_certs.last() {
      Some(Ok(cert)) if *cert == root_cert => (),
      _ => return Err("Invalid certificate chain"),
    }

    for (i, cert) in chain_certs.iter().enumerate().take(chain_certs.len() - 1) {
      let cert = cert.as_ref().map_err(|e| {
        shlog_error!("Failed to get certificate: {}", e);
        "Failed to get certificate"
      })?;
      let next_cert = chain_certs[i + 1].as_ref().map_err(|e| {
        shlog_error!("Failed to get next certificate: {}", e);
        "Failed to get next certificate"
      })?;
      let pub_key = next_cert.public_key().map_err(|e| {
        shlog_error!("Failed to get public key: {}", e);
        "Failed to get public key"
      })?;
      let valid = cert.verify(&pub_key).map_err(|e| {
        shlog_error!("Failed to verify certificate: {}", e);
        "Failed to verify certificate"
      })?;
      if !valid {
        return Err("Invalid certificate chain");
      }
    }

    Ok(Some(true.into()))
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[derive(shards::shard)]
#[shard_info(
  "X509.PublicKey",
  "Extracts a public key from an X509 certificate and outputs it as a PEM string"
)]
struct X509PublicKey {
  output: ClonedVar,
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
impl Default for X509PublicKey {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

// #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
#[shards::shard_impl]
impl Shard for X509PublicKey {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_STRING_OR_BYTE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, _ctx: &Context) -> Result<(), &str> {
    Ok(())
  }

  fn cleanup(&mut self, _ctx: Option<&Context>) -> Result<(), &str> {
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get the certificate from input
    let cert_data = if input.is_string() {
      let cert_pem: &str = input.try_into().unwrap();
      X509::from_pem(cert_pem.as_bytes()).map_err(|e| {
        shlog_error!("Failed to parse certificate: {}", e);
        "Failed to parse certificate"
      })?
    } else if input.is_bytes() {
      let cert_bytes: &[u8] = input.try_into().unwrap();
      X509::from_der(cert_bytes).map_err(|e| {
        shlog_error!("Failed to parse certificate: {}", e);
        "Failed to parse certificate"
      })?
    } else {
      return Err("Certificate must be either string or bytes");
    };

    // Extract the public key
    let public_key = cert_data.public_key().map_err(|e| {
      shlog_error!("Failed to extract public key: {}", e);
      "Failed to extract public key"
    })?;

    // Convert to PEM
    let pem = public_key.public_key_to_pem().map_err(|e| {
      shlog_error!("Failed to convert public key to PEM: {}", e);
      "Failed to convert public key to PEM"
    })?;

    // Convert to string and return
    let pem_string = String::from_utf8(pem).map_err(|_| "Failed to convert PEM to UTF-8 string")?;

    self.output = Var::ephemeral_string(&pem_string).into();
    Ok(Some(self.output.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_crypto_crypto(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  ecdsa::register_shards();
  hash::register_shards();
  signatures::register_shards();
  chachapoly::register_shards();

  register_shard::<MnemonicGenerate>();
  register_shard::<MnemonicToSeed>();

  argon::register_shards();

  // #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
  register_shard::<JwtDecode>();
  // #[cfg(not(any(target_arch = "wasm32", target_os = "windows")))]
  register_shard::<JwtVerify>();

  register_shard::<X509Verify>();
  register_shard::<X509PublicKey>();
}
