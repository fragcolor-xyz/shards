use sp_core::crypto::AccountId32;
use sp_keyring::AccountKeyring;
use sp_core::crypto::Pair;
use sp_runtime::MultiSigner;
use sp_core::ecdsa;
use sp_core::sr25519;

/*
(SR25519.PublicKey :Compressed true)
; assume ECDSA
(Substrate.AccountId)
*/

#[test]
fn test1() {
  let pair: sr25519::Pair = Pair::from_string("//Alice", None).unwrap();
  let ecdsa_pair: ecdsa::Pair = Pair::from_string("//Alice", None).unwrap();
  let accountId: AccountId32 = pair.public().into();
  // ecdsa::Public::from_raw();
}