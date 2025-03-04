/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

use der::Decode;
use der::Encode;
use hmac::Hmac;
use pbkdf2::pbkdf2;
use ring::signature::UnparsedPublicKey;
use rustls_pki_types::CertificateDer;
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
use x509_cert::Certificate;

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

    // Parse root certificate
    let root_cert_der = if self.root_cert.get().is_string() {
      let root_cert_pem: &str = self.root_cert.get().try_into().unwrap();
      pem_to_der(root_cert_pem).map_err(|_| "Failed to parse root certificate")?
    } else if self.root_cert.get().is_bytes() {
      let root_cert_bytes: &[u8] = self.root_cert.get().try_into().unwrap();
      CertificateDer::from(root_cert_bytes.to_vec())
    } else {
      return Err("Root certificate parameter is required");
    };

    let root_cert = Certificate::from_der(&root_cert_der).map_err(|e| {
      shlog_error!("Failed to parse root certificate: {}", e);
      "Failed to parse root certificate"
    })?;

    // Parse certificate chain
    let mut chain_certs = Vec::new();
    for pem in chain_pems.iter() {
      let cert_der = if pem.is_string() {
        let pem_str: &str = pem.as_ref().try_into().unwrap();
        pem_to_der(pem_str).map_err(|_| "Failed to parse certificate in chain")?
      } else if pem.is_bytes() {
        let pem_bytes: &[u8] = pem.as_ref().try_into().unwrap();
        CertificateDer::from(pem_bytes.to_vec())
      } else {
        return Err("Certificate must be either string or bytes");
      };

      let cert = Certificate::from_der(&cert_der).map_err(|e| {
        shlog_error!("Failed to parse certificate in chain: {}", e);
        "Failed to parse certificate in chain"
      })?;
      chain_certs.push(cert);
    }

    // Verify the certificate chain
    // This is simplified - a proper implementation would use webpki or rustls crate
    // for complete X.509 path validation

    // Check if the last cert matches the root cert
    if chain_certs.last().unwrap().tbs_certificate != root_cert.tbs_certificate {
      return Err("Last certificate in chain does not match root certificate");
    }

    // Verify each certificate against its issuer
    for i in 0..chain_certs.len() - 1 {
      let cert = &chain_certs[i];
      let issuer = &chain_certs[i + 1];

      // Get the public key of the issuer
      let spki = &issuer.tbs_certificate.subject_public_key_info;
      let public_key_info = spki.subject_public_key.raw_bytes();

      // Get the signature from the certificate
      let signature = &cert.signature.raw_bytes();
      let signed_data = cert
        .tbs_certificate
        .to_der()
        .map_err(|_| "Failed to serialize certificate data")?;

      // Use ring to verify the signature based on algorithm OID
      match cert.signature_algorithm.oid.to_string().as_str() {
        "1.2.840.113549.1.1.11" => {
          // sha256WithRSAEncryption - PKCS#1 v1.5 with SHA-256
          let alg = &ring::signature::RSA_PKCS1_2048_8192_SHA256;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("RSA signature verification failed: {:?}", e);
            return Err("RSA signature verification failed");
          }
        }
        "1.2.840.113549.1.1.12" => {
          // sha384WithRSAEncryption - PKCS#1 v1.5 with SHA-384
          let alg = &ring::signature::RSA_PKCS1_2048_8192_SHA384;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("RSA signature verification failed: {:?}", e);
            return Err("RSA signature verification failed");
          }
        }
        "1.2.840.113549.1.1.13" => {
          // sha512WithRSAEncryption - PKCS#1 v1.5 with SHA-512
          let alg = &ring::signature::RSA_PKCS1_2048_8192_SHA512;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("RSA signature verification failed: {:?}", e);
            return Err("RSA signature verification failed");
          }
        }
        "1.2.840.113549.1.1.10" => {
          // rsassa-pss - PSS with SHA-256 (most common)
          // Note: PSS requires parameter parsing to determine exact hash algorithm
          // This is a simplified version using SHA-256
          let alg = &ring::signature::RSA_PSS_2048_8192_SHA256;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("RSA-PSS signature verification failed: {:?}", e);
            return Err("RSA-PSS signature verification failed");
          }
        }
        "1.2.840.10045.4.3.2" => {
          // ecdsa-with-SHA256
          let alg = &ring::signature::ECDSA_P256_SHA256_ASN1;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("ECDSA P-256 signature verification failed: {:?}", e);
            return Err("ECDSA signature verification failed");
          }
        }
        "1.2.840.10045.4.3.3" => {
          // ecdsa-with-SHA384
          let alg = &ring::signature::ECDSA_P384_SHA384_ASN1;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("ECDSA P-384 signature verification failed: {:?}", e);
            return Err("ECDSA signature verification failed");
          }
        }
        "1.3.101.112" => {
          // ed25519
          let alg = &ring::signature::ED25519;
          let key = UnparsedPublicKey::new(alg, public_key_info);

          if let Err(e) = key.verify(signed_data.as_slice(), signature) {
            shlog_error!("Ed25519 signature verification failed: {:?}", e);
            return Err("Ed25519 signature verification failed");
          }
        }
        _ => {
          shlog_error!(
            "Unsupported signature algorithm: {}",
            cert.signature_algorithm.oid
          );
          return Err("Unsupported signature algorithm");
        }
      }

      // In a complete implementation, you would verify each certificate in the chain
      // using the appropriate signature verification algorithm from ring
    }

    // If we get here, verification succeeded
    Ok(Some(true.into()))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "X509.PublicKey",
  "Extracts a public key from an X509 certificate and outputs it as a PEM string"
)]
struct X509PublicKey {
  output: ClonedVar,
}

impl Default for X509PublicKey {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
    }
  }
}

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
    // Parse certificate
    let cert_der = if input.is_string() {
      let cert_pem: &str = input.try_into().unwrap();
      pem_to_der(cert_pem).map_err(|_| "Failed to parse certificate")?
    } else if input.is_bytes() {
      let cert_bytes: &[u8] = input.try_into().unwrap();
      CertificateDer::from(cert_bytes.to_vec())
    } else {
      return Err("Certificate must be either string or bytes");
    };

    let cert = Certificate::from_der(&cert_der).map_err(|e| {
      shlog_error!("Failed to parse certificate: {}", e);
      "Failed to parse certificate"
    })?;

    // Extract public key
    let public_key_info = &cert.tbs_certificate.subject_public_key_info;
    let public_key_der = public_key_info.to_der().map_err(|e| {
      shlog_error!("Failed to serialize public key: {}", e);
      "Failed to serialize public key"
    })?;

    // Convert to PEM format
    let pem = der_to_pem(&public_key_der, "PUBLIC KEY");

    self.output = Var::ephemeral_string(&pem).into();
    Ok(Some(self.output.0))
  }
}

// Helper function to convert PEM to DER
fn pem_to_der(pem_str: &str) -> Result<CertificateDer, &'static str> {
  // Basic PEM parsing
  let pem_lines: Vec<&str> = pem_str
    .lines()
    .filter(|line| !line.starts_with("-----"))
    .collect();

  let base64_data = pem_lines.join("");
  let der_data = base64::decode(&base64_data).map_err(|_| "Failed to decode base64 PEM data")?;

  Ok(CertificateDer::from(der_data))
}

// Helper function to convert DER to PEM
fn der_to_pem(der_data: &[u8], pem_type: &str) -> String {
  let mut result = format!("-----BEGIN {}-----\n", pem_type);

  // Convert to base64 with line wrapping at 64 characters
  let base64_data = base64::encode(der_data);
  for chunk in base64_data.as_bytes().chunks(64) {
    if let Ok(line) = std::str::from_utf8(chunk) {
      result.push_str(line);
      result.push('\n');
    }
  }

  result.push_str(&format!("-----END {}-----\n", pem_type));
  result
}

pub fn register_shards() {
  register_shard::<X509Verify>();
  register_shard::<X509PublicKey>();
}
