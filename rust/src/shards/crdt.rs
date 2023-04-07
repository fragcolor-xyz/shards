use lib0::any;
use yrs::types::ToJson;
use yrs::{Array, Doc, Map, MapPrelim, Transact};

pub fn sample1() {
  let doc = Doc::new();
  let map = doc.get_or_insert_map("map");
  let mut txn = doc.transact_mut();

  // insert value
  map.insert(&mut txn, "key1", "value1");

  // insert nested shared type
  let nested = map.insert(&mut txn, "key2", MapPrelim::from([("inner", "value2")]));
  nested.insert(&mut txn, "inner2", 100);

  assert_eq!(
    map.to_json(&txn),
    any!({
      "key1": "value1",
      "key2": {
        "inner": "value2",
        "inner2": 100
      }
    })
  );

  // get value
  assert_eq!(map.get(&txn, "key1"), Some("value1".into()));

  // remove entry
  map.remove(&mut txn, "key1");
  assert_eq!(map.get(&txn, "key1"), None);
}

pub fn sample2() {
  let doc = Doc::new();
  let array = doc.get_or_insert_array("array");
  let mut txn = doc.transact_mut();

  // insert single scalar value
  array.insert(&mut txn, 0, "value");
  array.remove_range(&mut txn, 0, 1);

  assert_eq!(array.len(&txn), 0);

  // insert multiple values at once
  array.insert_range(&mut txn, 0, ["a", "b", "c"]);
  assert_eq!(array.len(&txn), 3);

  // get value
  let value = array.get(&txn, 1);
  assert_eq!(value, Some("b".into()));

  // insert nested shared types
  let map = array.insert(&mut txn, 1, MapPrelim::from([("key1", "value1")]));
  map.insert(&mut txn, "key2", "value2");

  assert_eq!(
    array.to_json(&txn),
    any!([
      "a",
      { "key1": "value1", "key2": "value2" },
      "b",
      "c"
    ])
  );
}
