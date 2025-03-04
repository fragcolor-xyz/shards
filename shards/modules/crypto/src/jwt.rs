/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

use serde_json::Value;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ParamVar;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_TYPES;
use shards::types::STRING_TYPES;
use std::convert::TryInto;
use {
  jsonwebtoken::jwk::Jwk,
  jsonwebtoken::jwk::KeyAlgorithm,
  jsonwebtoken::{
    decode, decode_header, encode, Algorithm, DecodingKey, EncodingKey, Header, Validation,
  },
};

#[derive(shards::shard)]
#[shard_info("Jwt.Decode", "Decodes a JWT token")]
struct JwtDecode {
  output: ClonedVar,

  #[shard_param("Jwk", "The Key in JWK format to use for decoding the token.", [common_type::string, common_type::string_var])]
  jwk: ParamVar,

  #[shard_param("Audience", "The audience to use for decoding the token.", [common_type::string, common_type::string_var])]
  audience: ParamVar,
}

impl Default for JwtDecode {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      jwk: ParamVar::default(),
      audience: ParamVar::default(),
    }
  }
}

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

impl Default for JwtVerify {
  fn default() -> Self {
    Self {
      jwk: ParamVar::default(),
      pem_ec: ParamVar::default(),
      pem_rsa: ParamVar::default(),
    }
  }
}

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

#[derive(shards::shard)]
#[shard_info("Jwt.Encode", "Encodes a JSON payload into a JWT token")]
struct JwtEncode {
  output: ClonedVar,

  #[shard_param("PemEc", "The Key in PEM ECDSA format to use for encoding the token.", [common_type::none, common_type::string, common_type::string_var])]
  pem_ec: ParamVar,

  #[shard_param("PemRsa", "The Key in PEM RSA format to use for encoding the token.", [common_type::none, common_type::string, common_type::string_var])]
  pem_rsa: ParamVar,

  #[shard_param("Algorithm", "The algorithm to use for encoding (RS256, RS384, ES256, ES384).", [common_type::string, common_type::string_var])]
  algorithm: ParamVar,

  #[shard_param("Audience", "The audience to encode in the token claims.", [common_type::none, common_type::string, common_type::string_var])]
  audience: ParamVar,

  #[shard_param("Issuer", "The issuer to encode in the token claims.", [common_type::none, common_type::string, common_type::string_var])]
  issuer: ParamVar,

  #[shard_param("ExpirationTime", "The expiration time in seconds from now.", [common_type::none, common_type::int, common_type::int_var])]
  expiration_time: ParamVar,
}

impl Default for JwtEncode {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      pem_ec: ParamVar::default(),
      pem_rsa: ParamVar::default(),
      algorithm: ParamVar::default(),
      audience: ParamVar::default(),
      issuer: ParamVar::default(),
      expiration_time: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for JwtEncode {
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
    // Parse the algorithm
    let alg_str: &str = self.algorithm.get().try_into().unwrap();
    let algorithm = match alg_str.to_uppercase().as_str() {
      "RS256" => Algorithm::RS256,
      "RS384" => Algorithm::RS384,
      "ES256" => Algorithm::ES256,
      "ES384" => Algorithm::ES384,
      _ => return Err("Unsupported algorithm"),
    };

    // Create the encoding key based on the provided key type
    let encoding_key = if !self.pem_ec.get().is_none() {
      let pem: &str = self.pem_ec.get().try_into().unwrap();
      EncodingKey::from_ec_pem(pem.as_bytes()).map_err(|e| {
        shlog_error!("Invalid PEM ECDSA key: {}", e);
        "Invalid PEM ECDSA key"
      })?
    } else if !self.pem_rsa.get().is_none() {
      let pem: &str = self.pem_rsa.get().try_into().unwrap();
      EncodingKey::from_rsa_pem(pem.as_bytes()).map_err(|e| {
        shlog_error!("Invalid PEM RSA key: {}", e);
        "Invalid PEM RSA key"
      })?
    } else {
      return Err("No valid key provided. Please provide either a PEM ECDSA or RSA key.");
    };

    // Parse the input JSON
    let claims_json: &str = input.try_into().unwrap();
    let mut claims: Value = serde_json::from_str(claims_json).map_err(|e| {
      shlog_error!("Invalid JSON input: {}", e);
      "Invalid JSON input"
    })?;

    // Ensure claims is an object
    if !claims.is_object() {
      return Err("Input must be a JSON object");
    }

    // Add standard claims if they don't exist
    let claims_obj = claims.as_object_mut().unwrap();

    // Add audience
    if !self.audience.get().is_none() {
      let audience: &str = self.audience.get().try_into().unwrap();
      claims_obj.insert("aud".to_string(), Value::String(audience.to_string()));
    }

    // Add issuer if provided
    if !self.issuer.get().is_none() {
      let issuer: &str = self.issuer.get().try_into().unwrap();
      claims_obj.insert("iss".to_string(), Value::String(issuer.to_string()));
    }

    // Add expiration time if provided
    if !self.expiration_time.get().is_none() {
      let exp_seconds: i64 = self.expiration_time.get().try_into().unwrap();
      let current_time = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(|_| "System time error")?
        .as_secs() as i64;
      let exp_time = current_time + exp_seconds;
      claims_obj.insert("exp".to_string(), Value::Number(exp_time.into()));
    }

    // Create header with the algorithm
    let header = Header::new(algorithm);

    // Encode the token
    let token = encode(&header, &claims, &encoding_key).map_err(|e| {
      shlog_error!("Failed to encode token: {}", e);
      "Failed to encode token"
    })?;

    self.output = Var::ephemeral_string(&token).into();

    Ok(Some(self.output.0))
  }
}

pub fn register_shards() {
  register_shard::<JwtDecode>();
  register_shard::<JwtVerify>();
  register_shard::<JwtEncode>();
}
