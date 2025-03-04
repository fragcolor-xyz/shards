/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

use base64::Engine;
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
use std::collections::HashSet;
use std::convert::TryInto;

use jwt_simple::prelude::Duration;
use jwt_simple::prelude::*;

// Utility function to decode base64url-encoded strings
fn decode_base64url(input: &str) -> Result<Vec<u8>, &'static str> {
  base64::engine::general_purpose::URL_SAFE_NO_PAD
    .decode(input)
    .map_err(|_| "Invalid base64url encoding")
}

// Utility function to convert RSA components to DER format
fn rsa_der_from_components(n: &[u8], e: &[u8]) -> Result<Vec<u8>, &'static str> {
  // This is a simplified ASN.1 DER encoder for RSA public keys
  // Format: RSAPublicKey ::= SEQUENCE { modulus INTEGER, publicExponent INTEGER }

  // Helper function to encode an integer in DER format
  fn encode_der_integer(data: &[u8]) -> Vec<u8> {
    let mut result = vec![0x02]; // INTEGER tag

    // For DER encoding, we need to skip leading zeros, except we need one zero
    // if the high bit is set to indicate a positive integer
    let mut start = 0;
    while start < data.len() && data[start] == 0 {
      start += 1;
    }

    // If all bytes are zero, encode as a single zero
    if start == data.len() {
      return vec![0x02, 0x01, 0x00];
    }

    // Go back one if we need a leading zero for the high bit
    if start < data.len() && (data[start] & 0x80) != 0 {
      if start > 0 {
        start -= 1;
      } else {
        // Prepend a zero if needed and no zeros available
        let mut content = vec![0x00];
        content.extend_from_slice(data);
        let length = content.len();

        // Handle multi-byte length encoding for long integers
        if length < 128 {
          result.push(length as u8);
        } else {
          // Calculate how many bytes we need for the length
          let mut len_bytes = Vec::new();
          let mut len_val = length;
          while len_val > 0 {
            len_bytes.push((len_val & 0xFF) as u8);
            len_val >>= 8;
          }
          len_bytes.reverse(); // Big-endian format

          result.push(0x80 | len_bytes.len() as u8); // Long form length indicator
          result.extend_from_slice(&len_bytes);
        }

        result.extend_from_slice(&content);
        return result;
      }
    }

    // Calculate length of integer without excessive leading zeros
    let content_len = data.len() - start;

    // Handle multi-byte length encoding
    if content_len < 128 {
      result.push(content_len as u8);
    } else {
      // Calculate how many bytes we need for the length
      let mut len_bytes = Vec::new();
      let mut len_val = content_len;
      while len_val > 0 {
        len_bytes.push((len_val & 0xFF) as u8);
        len_val >>= 8;
      }
      len_bytes.reverse(); // Big-endian format

      result.push(0x80 | len_bytes.len() as u8); // Long form length indicator
      result.extend_from_slice(&len_bytes);
    }

    result.extend_from_slice(&data[start..]); // Content

    result
  }

  // Helper function to encode the length field in DER
  fn encode_der_length(length: usize) -> Vec<u8> {
    if length < 128 {
      // Short form
      return vec![length as u8];
    } else {
      // Long form
      let mut len_bytes = Vec::new();
      let mut len_val = length;
      while len_val > 0 {
        len_bytes.push((len_val & 0xFF) as u8);
        len_val >>= 8;
      }
      len_bytes.reverse(); // Big-endian format

      let mut result = vec![0x80 | len_bytes.len() as u8]; // Long form length indicator
      result.extend_from_slice(&len_bytes);
      return result;
    }
  }

  // Encode modulus (n) and exponent (e) as DER integers
  let der_n = encode_der_integer(n);
  let der_e = encode_der_integer(e);

  // Create RSA public key sequence
  let mut inner_seq = vec![0x30]; // SEQUENCE tag
  let seq_content_len = der_n.len() + der_e.len();
  let seq_len_bytes = encode_der_length(seq_content_len);
  inner_seq.extend_from_slice(&seq_len_bytes);
  inner_seq.extend_from_slice(&der_n); // Modulus
  inner_seq.extend_from_slice(&der_e); // Exponent

  // Algorithm identifier sequence: OID + NULL
  let mut alg_seq = vec![0x30]; // SEQUENCE tag
  let alg_oid = vec![
    0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
  ]; // RSA OID (1.2.840.113549.1.1.1)
  let null_params = vec![0x05, 0x00]; // NULL
  let alg_content_len = alg_oid.len() + null_params.len();
  let alg_len_bytes = encode_der_length(alg_content_len);
  alg_seq.extend_from_slice(&alg_len_bytes);
  alg_seq.extend_from_slice(&alg_oid);
  alg_seq.extend_from_slice(&null_params);

  // BIT STRING for the key
  let mut bit_string = vec![0x03]; // BIT STRING tag
  let bs_content_len = inner_seq.len() + 1; // +1 for unused bits byte
  let bs_len_bytes = encode_der_length(bs_content_len);
  bit_string.extend_from_slice(&bs_len_bytes);
  bit_string.push(0x00); // Zero unused bits
  bit_string.extend_from_slice(&inner_seq);

  // Final outer SEQUENCE (SubjectPublicKeyInfo)
  let mut spki = vec![0x30]; // SEQUENCE tag
  let spki_content_len = alg_seq.len() + bit_string.len();
  let spki_len_bytes = encode_der_length(spki_content_len);
  spki.extend_from_slice(&spki_len_bytes);
  spki.extend_from_slice(&alg_seq);
  spki.extend_from_slice(&bit_string);

  Ok(spki)
}

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

    // Parse JWK and determine algorithm type
    let jwk_value: Value = serde_json::from_str(jwk).map_err(|e| {
      shlog_error!("Invalid JWK: {}", e);
      "Invalid JWK format"
    })?;

    // Extract kty and alg from JWK
    let kty = jwk_value["kty"].as_str().ok_or("Missing kty in JWK")?;
    let alg = jwk_value["alg"].as_str().ok_or("Missing alg in JWK")?;

    // Create the appropriate verification key
    let audience: &str = self.audience.get().try_into().unwrap();
    let mut audience_set = HashSet::new();
    audience_set.insert(audience.to_string());

    let token: &str = input.try_into().unwrap();
    let claims_value: JWTClaims<Value>;

    // Handle different key types and algorithms
    match (kty, alg) {
      ("EC", "ES256") => {
        // Extract EC key components from JWK
        let x_b64 = jwk_value["x"].as_str().ok_or("Missing x in EC JWK")?;
        let y_b64 = jwk_value["y"].as_str().ok_or("Missing y in EC JWK")?;

        // Decode base64url to bytes
        let x = decode_base64url(x_b64).map_err(|_| {
          shlog_error!("Invalid x coordinate encoding");
          "Invalid x coordinate encoding"
        })?;
        let y = decode_base64url(y_b64).map_err(|_| {
          shlog_error!("Invalid y coordinate encoding");
          "Invalid y coordinate encoding"
        })?;

        // Combine into uncompressed point format (0x04 | x | y)
        let mut point = vec![0x04];
        point.extend_from_slice(&x);
        point.extend_from_slice(&y);

        // Create EC key from bytes
        let public_key = ES256PublicKey::from_bytes(&point).map_err(|e| {
          shlog_error!("Invalid ES256 key: {}", e);
          "Invalid ES256 key"
        })?;

        let options = VerificationOptions {
          allowed_audiences: Some(audience_set),
          ..Default::default()
        };

        let claims = public_key
          .verify_token::<Value>(token, Some(options))
          .map_err(|e| {
            shlog_error!("Invalid token: {}", e);
            "Invalid token"
          })?;

        claims_value = claims;
      }
      ("EC", "ES384") => {
        // Extract EC key components from JWK
        let x_b64 = jwk_value["x"].as_str().ok_or("Missing x in EC JWK")?;
        let y_b64 = jwk_value["y"].as_str().ok_or("Missing y in EC JWK")?;

        // Decode base64url to bytes
        let x = decode_base64url(x_b64).map_err(|_| {
          shlog_error!("Invalid x coordinate encoding");
          "Invalid x coordinate encoding"
        })?;
        let y = decode_base64url(y_b64).map_err(|_| {
          shlog_error!("Invalid y coordinate encoding");
          "Invalid y coordinate encoding"
        })?;

        // Combine into uncompressed point format (0x04 | x | y)
        let mut point = vec![0x04];
        point.extend_from_slice(&x);
        point.extend_from_slice(&y);

        // Create EC key from bytes
        let public_key = ES384PublicKey::from_bytes(&point).map_err(|e| {
          shlog_error!("Invalid ES384 key: {}", e);
          "Invalid ES384 key"
        })?;

        let options = VerificationOptions {
          allowed_audiences: Some(audience_set),
          ..Default::default()
        };

        let claims = public_key
          .verify_token::<Value>(token, Some(options))
          .map_err(|e| {
            shlog_error!("Invalid token: {}", e);
            "Invalid token"
          })?;

        claims_value = claims;
      }
      ("RSA", "RS256") => {
        // Extract RSA key components from JWK
        let n_b64 = jwk_value["n"].as_str().ok_or("Missing n in RSA JWK")?;
        let e_b64 = jwk_value["e"].as_str().ok_or("Missing e in RSA JWK")?;

        // Decode base64url to bytes
        let n = decode_base64url(n_b64).map_err(|_| {
          shlog_error!("Invalid n parameter encoding");
          "Invalid n parameter encoding"
        })?;
        let e = decode_base64url(e_b64).map_err(|_| {
          shlog_error!("Invalid e parameter encoding");
          "Invalid e parameter encoding"
        })?;

        // Convert to DER format
        let der = rsa_der_from_components(&n, &e).map_err(|_| {
          shlog_error!("Failed to create DER from RSA components");
          "Failed to create DER from RSA components"
        })?;

        // Create RSA key from DER
        let public_key = RS256PublicKey::from_der(&der).map_err(|e| {
          shlog_error!("Invalid RS256 key: {}", e);
          "Invalid RS256 key"
        })?;

        let options = VerificationOptions {
          allowed_audiences: Some(audience_set),
          ..Default::default()
        };

        let claims = public_key
          .verify_token::<Value>(token, Some(options))
          .map_err(|e| {
            shlog_error!("Invalid token: {}", e);
            "Invalid token"
          })?;

        claims_value = claims;
      }
      ("RSA", "RS384") => {
        // Extract RSA key components from JWK
        let n_b64 = jwk_value["n"].as_str().ok_or("Missing n in RSA JWK")?;
        let e_b64 = jwk_value["e"].as_str().ok_or("Missing e in RSA JWK")?;

        // Decode base64url to bytes
        let n = decode_base64url(n_b64).map_err(|_| {
          shlog_error!("Invalid n parameter encoding");
          "Invalid n parameter encoding"
        })?;
        let e = decode_base64url(e_b64).map_err(|_| {
          shlog_error!("Invalid e parameter encoding");
          "Invalid e parameter encoding"
        })?;

        // Convert to DER format
        let der = rsa_der_from_components(&n, &e).map_err(|_| {
          shlog_error!("Failed to create DER from RSA components");
          "Failed to create DER from RSA components"
        })?;

        // Create RSA key from DER
        let public_key = RS384PublicKey::from_der(&der).map_err(|e| {
          shlog_error!("Invalid RS384 key: {}", e);
          "Invalid RS384 key"
        })?;

        let options = VerificationOptions {
          allowed_audiences: Some(audience_set),
          ..Default::default()
        };

        let claims = public_key
          .verify_token::<Value>(token, Some(options))
          .map_err(|e| {
            shlog_error!("Invalid token: {}", e);
            "Invalid token"
          })?;

        claims_value = claims;
      }
      _ => return Err("Unsupported key type or algorithm"),
    }

    let json_string = serde_json::to_string(&claims_value).map_err(|e| {
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
    let token: &str = input.try_into().unwrap();

    // Determine token algorithm by parsing the header
    let parts: Vec<&str> = token.split('.').collect();
    if parts.len() != 3 {
      return Err("Invalid token format");
    }

    let header_json = decode_base64url(parts[0]).map_err(|_| "Invalid header encoding")?;
    let header: Value =
      serde_json::from_slice(&header_json).map_err(|_| "Invalid header format")?;
    let alg = header["alg"]
      .as_str()
      .ok_or("Missing algorithm in header")?;

    // Options for verification (ignore expiration and audience)
    let options = VerificationOptions {
      accept_future: true,
      time_tolerance: Some(Duration::from_secs(u64::MAX / 2)), // Effectively disables expiration checking
      ..Default::default()
    };

    // Try to verify with the provided key
    let result = if !self.jwk.get().is_none() {
      let jwk: &str = self.jwk.get().try_into().unwrap();
      let jwk_value: Value = serde_json::from_str(jwk).map_err(|e| {
        shlog_error!("Invalid JWK: {}", e);
        "Invalid JWK format"
      })?;

      // Match the algorithm from token with JWK type
      match alg {
        "ES256" => {
          // Extract EC key components from JWK
          let x_b64 = jwk_value["x"].as_str().ok_or("Missing x in EC JWK")?;
          let y_b64 = jwk_value["y"].as_str().ok_or("Missing y in EC JWK")?;

          // Decode base64url to bytes
          let x = decode_base64url(x_b64).map_err(|_| {
            shlog_error!("Invalid x coordinate encoding");
            "Invalid x coordinate encoding"
          })?;
          let y = decode_base64url(y_b64).map_err(|_| {
            shlog_error!("Invalid y coordinate encoding");
            "Invalid y coordinate encoding"
          })?;

          // Combine into uncompressed point format (0x04 | x | y)
          let mut point = vec![0x04];
          point.extend_from_slice(&x);
          point.extend_from_slice(&y);

          // Create EC key from bytes
          let key = ES256PublicKey::from_bytes(&point).map_err(|e| {
            shlog_error!("Invalid ES256 key: {}", e);
            "Invalid ES256 key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        "ES384" => {
          // Extract EC key components from JWK
          let x_b64 = jwk_value["x"].as_str().ok_or("Missing x in EC JWK")?;
          let y_b64 = jwk_value["y"].as_str().ok_or("Missing y in EC JWK")?;

          // Decode base64url to bytes
          let x = decode_base64url(x_b64).map_err(|_| {
            shlog_error!("Invalid x coordinate encoding");
            "Invalid x coordinate encoding"
          })?;
          let y = decode_base64url(y_b64).map_err(|_| {
            shlog_error!("Invalid y coordinate encoding");
            "Invalid y coordinate encoding"
          })?;

          // Combine into uncompressed point format (0x04 | x | y)
          let mut point = vec![0x04];
          point.extend_from_slice(&x);
          point.extend_from_slice(&y);

          // Create EC key from bytes
          let key = ES384PublicKey::from_bytes(&point).map_err(|e| {
            shlog_error!("Invalid ES384 key: {}", e);
            "Invalid ES384 key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        "RS256" => {
          // Extract RSA key components from JWK
          let n_b64 = jwk_value["n"].as_str().ok_or("Missing n in RSA JWK")?;
          let e_b64 = jwk_value["e"].as_str().ok_or("Missing e in RSA JWK")?;

          // Decode base64url to bytes
          let n = decode_base64url(n_b64).map_err(|_| {
            shlog_error!("Invalid n parameter encoding");
            "Invalid n parameter encoding"
          })?;
          let e = decode_base64url(e_b64).map_err(|_| {
            shlog_error!("Invalid e parameter encoding");
            "Invalid e parameter encoding"
          })?;

          // Convert to DER format
          let der = rsa_der_from_components(&n, &e).map_err(|_| {
            shlog_error!("Failed to create DER from RSA components");
            "Failed to create DER from RSA components"
          })?;

          // Create RSA key from DER
          let key = RS256PublicKey::from_der(&der).map_err(|e| {
            shlog_error!("Invalid RS256 key: {}", e);
            "Invalid RS256 key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        "RS384" => {
          // Extract RSA key components from JWK
          let n_b64 = jwk_value["n"].as_str().ok_or("Missing n in RSA JWK")?;
          let e_b64 = jwk_value["e"].as_str().ok_or("Missing e in RSA JWK")?;

          // Decode base64url to bytes
          let n = decode_base64url(n_b64).map_err(|_| {
            shlog_error!("Invalid n parameter encoding");
            "Invalid n parameter encoding"
          })?;
          let e = decode_base64url(e_b64).map_err(|_| {
            shlog_error!("Invalid e parameter encoding");
            "Invalid e parameter encoding"
          })?;

          // Convert to DER format
          let der = rsa_der_from_components(&n, &e).map_err(|_| {
            shlog_error!("Failed to create DER from RSA components");
            "Failed to create DER from RSA components"
          })?;

          // Create RSA key from DER
          let key = RS384PublicKey::from_der(&der).map_err(|e| {
            shlog_error!("Invalid RS384 key: {}", e);
            "Invalid RS384 key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        _ => return Err("Unsupported algorithm in token"),
      }
    } else if !self.pem_ec.get().is_none() {
      let pem: &str = self.pem_ec.get().try_into().unwrap();

      // Match the algorithm with EC key type
      match alg {
        "ES256" => {
          let key = ES256PublicKey::from_pem(pem).map_err(|e| {
            shlog_error!("Invalid PEM ECDSA key: {}", e);
            "Invalid PEM ECDSA key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        "ES384" => {
          let key = ES384PublicKey::from_pem(pem).map_err(|e| {
            shlog_error!("Invalid PEM ECDSA key: {}", e);
            "Invalid PEM ECDSA key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        _ => return Err("Algorithm in token does not match EC key type"),
      }
    } else if !self.pem_rsa.get().is_none() {
      let pem: &str = self.pem_rsa.get().try_into().unwrap();

      // Match the algorithm with RSA key type
      match alg {
        "RS256" => {
          let key = RS256PublicKey::from_pem(pem).map_err(|e| {
            shlog_error!("Invalid PEM RSA key: {}", e);
            "Invalid PEM RSA key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        "RS384" => {
          let key = RS384PublicKey::from_pem(pem).map_err(|e| {
            shlog_error!("Invalid PEM RSA key: {}", e);
            "Invalid PEM RSA key"
          })?;
          key.verify_token::<Value>(token, Some(options)).is_ok()
        }
        _ => return Err("Algorithm in token does not match RSA key type"),
      }
    } else {
      return Err("No key provided");
    };

    Ok(Some(result.into()))
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

    // Parse the input JSON
    let claims_json: &str = input.try_into().unwrap();
    let claims_value: Value = serde_json::from_str(claims_json).map_err(|e| {
      shlog_error!("Invalid JSON input: {}", e);
      "Invalid JSON input"
    })?;

    // Ensure claims is an object
    if !claims_value.is_object() {
      return Err("Input must be a JSON object");
    }

    // Create a JWTClaims object
    let mut claims = Claims::with_custom_claims(claims_value, Duration::from_secs(0));

    // Add audience if provided
    if !self.audience.get().is_none() {
      let audience: &str = self.audience.get().try_into().unwrap();
      claims.audiences = Some(Audiences::AsString(audience.to_string()));
    }

    // Add issuer if provided
    if !self.issuer.get().is_none() {
      let issuer: &str = self.issuer.get().try_into().unwrap();
      claims.issuer = Some(issuer.to_string());
    }

    // Add expiration time if provided
    if !self.expiration_time.get().is_none() {
      let exp_seconds: i64 = self.expiration_time.get().try_into().unwrap();
      let current_time = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_err(|_| "System time error")?
        .as_secs() as u64;
      let exp_time = current_time + exp_seconds as u64;
      claims.expires_at = Some(exp_time.into());
    }

    // Create the token based on the algorithm and key type
    let token = match alg_str.to_uppercase().as_str() {
      "ES256" => {
        if self.pem_ec.get().is_none() {
          return Err("ES256 algorithm requires an EC key");
        }
        let pem: &str = self.pem_ec.get().try_into().unwrap();
        let key_pair = ES256KeyPair::from_pem(pem).map_err(|e| {
          shlog_error!("Invalid PEM ECDSA key: {}", e);
          "Invalid PEM ECDSA key"
        })?;
        key_pair.sign(claims)
      }
      "ES384" => {
        if self.pem_ec.get().is_none() {
          return Err("ES384 algorithm requires an EC key");
        }
        let pem: &str = self.pem_ec.get().try_into().unwrap();
        let key_pair = ES384KeyPair::from_pem(pem).map_err(|e| {
          shlog_error!("Invalid PEM ECDSA key: {}", e);
          "Invalid PEM ECDSA key"
        })?;
        key_pair.sign(claims)
      }
      "RS256" => {
        if self.pem_rsa.get().is_none() {
          return Err("RS256 algorithm requires an RSA key");
        }
        let pem: &str = self.pem_rsa.get().try_into().unwrap();
        let key_pair = RS256KeyPair::from_pem(pem).map_err(|e| {
          shlog_error!("Invalid PEM RSA key: {}", e);
          "Invalid PEM RSA key"
        })?;
        key_pair.sign(claims)
      }
      "RS384" => {
        if self.pem_rsa.get().is_none() {
          return Err("RS384 algorithm requires an RSA key");
        }
        let pem: &str = self.pem_rsa.get().try_into().unwrap();
        let key_pair = RS384KeyPair::from_pem(pem).map_err(|e| {
          shlog_error!("Invalid PEM RSA key: {}", e);
          "Invalid PEM RSA key"
        })?;
        key_pair.sign(claims)
      }
      _ => return Err("Unsupported algorithm"),
    }
    .map_err(|e| {
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
