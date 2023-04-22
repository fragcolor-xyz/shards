use crdts::{CmRDT, CvRDT, MVReg, Map, Orswot};

use crate::types::{ClonedVar, Var};

use serde::{Deserialize, Serialize};

pub fn crdts_test() {
  let mut friend_map: Map<&str, Orswot<&str, u8>, u8> = Map::new();

  let read_ctx = friend_map.len(); // we read anything from the map to get a add context
  friend_map.apply(
    friend_map.update("bob", read_ctx.derive_add_ctx(1), |set, ctx| {
      set.add("janet", ctx)
    }),
  );

  let mut friend_map_on_2nd_device = friend_map.clone();

  // the map on the 2nd devices adds 'erik' to `bob`'s friends
  friend_map_on_2nd_device.apply(friend_map_on_2nd_device.update(
    "bob",
    friend_map_on_2nd_device.len().derive_add_ctx(2),
    |set, c| set.add("erik", c),
  ));

  // Meanwhile, on the first device we remove
  // the entire 'bob' entry from the friend map.
  friend_map.apply(friend_map.rm("bob", friend_map.get(&"bob").derive_rm_ctx()));

  assert!(friend_map.get(&"bob").val.is_none());

  // once these two devices synchronize...
  let friend_map_snapshot = friend_map.clone();
  let friend_map_on_2nd_device_snapshot = friend_map_on_2nd_device.clone();

  friend_map.merge(friend_map_on_2nd_device_snapshot);
  friend_map_on_2nd_device.merge(friend_map_snapshot);
  assert_eq!(friend_map, friend_map_on_2nd_device);

  // ... we see that "bob" is present but only
  // contains `erik`.
  //
  // This is because the `erik` entry was not
  // seen by the first device when it deleted
  // the entry.
  let bobs_friends = friend_map
    .get(&"bob")
    .val
    .map(|set| set.read().val)
    .map(|hashmap| hashmap.into_iter().collect::<Vec<_>>());

  assert_eq!(bobs_friends, Some(vec!["erik"]));
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Hash, Ord, Serialize, Deserialize)]
struct Bytes32([u8; 32]);

pub fn crdts_test2() {
  let actor1 = Bytes32([1; 32]);
  let actor2 = Bytes32([2; 32]);

  let mut friend_map: Map<&str, MVReg<Var, Bytes32>, Bytes32> = Map::new();

  let svar1: Var = Var::ephemeral_string("janet\0");

  let read_ctx = friend_map.len(); // we read anything from the map to get a add context
  friend_map.apply(
    friend_map.update("bob", read_ctx.derive_add_ctx(actor1), |r, c| {
      r.write(svar1, c)
    }),
  );

  assert_eq!(friend_map.get(&"bob").val.unwrap().read().val, vec![svar1]);

  let mut friend_map_on_2nd_device = friend_map.clone();

  let svar2: Var = Var::ephemeral_string("erik\0");

  // the map on the 2nd devices adds 'erik' to `bob`'s friends
  friend_map_on_2nd_device.apply(friend_map_on_2nd_device.update(
    "bob",
    friend_map_on_2nd_device.len().derive_add_ctx(actor2),
    |r, c| r.write(svar2, c),
  ));

  assert_eq!(
    friend_map_on_2nd_device.get(&"bob").val.unwrap().read().val,
    vec![svar2]
  );

  // // Meanwhile, on the first device we remove
  // // the entire 'bob' entry from the friend map.
  // friend_map.apply(friend_map.rm("bob", friend_map.get(&"bob").derive_rm_ctx()));

  // assert!(friend_map.get(&"bob").val.is_none());

  // once these two devices synchronize...
  let friend_map_snapshot = friend_map.clone();
  let friend_map_on_2nd_device_snapshot = friend_map_on_2nd_device.clone();

  assert_eq!(
    friend_map_snapshot.get(&"bob").val.unwrap().read().val,
    vec![svar1]
  );
  assert_eq!(
    friend_map_on_2nd_device_snapshot
      .get(&"bob")
      .val
      .unwrap()
      .read()
      .val,
    vec![svar2]
  );

  friend_map.merge(friend_map_on_2nd_device_snapshot);
  friend_map_on_2nd_device.merge(friend_map_snapshot);
  assert_eq!(friend_map, friend_map_on_2nd_device);

  assert_eq!(friend_map.get(&"bob").val.unwrap().read().val, vec![svar2]);
  assert_eq!(
    friend_map_on_2nd_device.get(&"bob").val.unwrap().read().val,
    vec![svar2]
  );

  let _encoded: Vec<u8> = bincode::serialize(&friend_map).unwrap();
}

pub fn crdts_test3() {
  let mut friend_map: Map<&str, MVReg<&str, u64>, u64> = Map::new();

  let svar1 = "janet";

  let read_ctx = friend_map.len(); // we read anything from the map to get a add context
  friend_map.apply(friend_map.update("bob", read_ctx.derive_add_ctx(1), |r, c| r.write(svar1, c)));

  assert_eq!(friend_map.get(&"bob").val.unwrap().read().val, vec![svar1]);

  let mut friend_map_on_2nd_device = friend_map.clone();
  // let mut friend_map_on_2nd_device: Map<&str, MVReg<&str, u64>, u64> = Map::new();

  let svar2 = "erik";

  // the map on the 2nd devices adds 'erik' to `bob`'s friends
  friend_map_on_2nd_device.apply(friend_map_on_2nd_device.update(
    "bob",
    friend_map_on_2nd_device.len().derive_add_ctx(2),
    |r, c| r.write(svar2, c),
  ));

  assert_eq!(
    friend_map_on_2nd_device.get(&"bob").val.unwrap().read().val,
    vec![svar2]
  );

  // // Meanwhile, on the first device we remove
  // // the entire 'bob' entry from the friend map.
  // friend_map.apply(friend_map.rm("bob", friend_map.get(&"bob").derive_rm_ctx()));

  // assert!(friend_map.get(&"bob").val.is_none());

  // once these two devices synchronize...
  let friend_map_snapshot = friend_map.clone();
  let friend_map_on_2nd_device_snapshot = friend_map_on_2nd_device.clone();

  assert_eq!(
    friend_map_snapshot.get(&"bob").val.unwrap().read().val,
    vec![svar1]
  );
  assert_eq!(
    friend_map_on_2nd_device_snapshot
      .get(&"bob")
      .val
      .unwrap()
      .read()
      .val,
    vec![svar2]
  );

  friend_map.merge(friend_map_on_2nd_device_snapshot);
  friend_map_on_2nd_device.merge(friend_map_snapshot);
  assert_eq!(friend_map, friend_map_on_2nd_device);

  assert_eq!(friend_map.get(&"bob").val.unwrap().read().val, vec![svar2]);
  assert_eq!(
    friend_map_on_2nd_device.get(&"bob").val.unwrap().read().val,
    vec![svar2]
  );
}
