use lib0::any;
use yrs::types::ToJson;
use yrs::*;
use yrs::types::text::YChange;
use std::collections::HashMap;
use yrs::updates::decoder::Decode;

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

pub fn sample3() {
  let doc = Doc::new();
  let text = doc.get_or_insert_text("name");
  // every operation in Yrs happens in scope of a transaction
  let mut txn = doc.transact_mut();
  // append text to our collaborative document
  text.push(&mut txn, "Hello from yrs!");
  // add formatting section to part of the text
  text.format(
    &mut txn,
    11,
    3,
    HashMap::from([("link".into(), "https://github.com/y-crdt/y-crdt".into())]),
  );

  // simulate update with remote peer
  let remote_doc = Doc::new();
  let remote_text = remote_doc.get_or_insert_text("name");
  let mut remote_txn = remote_doc.transact_mut();

  let remote2_doc = Doc::new();
  let remote2_text = remote2_doc.get_or_insert_text("name");
  let mut remote2_txn = remote2_doc.transact_mut();
  remote2_text.push(&mut remote2_txn, "Another hello from yrs!");

  // in order to exchange data with other documents
  // we first need to create a state vector
  let state_vector = remote_txn.state_vector();

  // now compute a differential update based on remote document's
  // state vector
  let bytes = txn.encode_diff_v1(&state_vector);

  // both update and state vector are serializable, we can pass them
  // over the wire now apply update to a remote document
  let update = Update::decode_v1(&bytes).unwrap();
  remote_txn.apply_update(update);

  // try to apply on a different doc with diff state
  let update = Update::decode_v1(&bytes).unwrap();
  remote2_txn.apply_update(update);

  // display raw text (no attributes)
  println!("{}", remote_text.get_string(&remote_txn));

  println!("{}", remote2_text.get_string(&remote2_txn));

  // create sequence of text chunks with optional format attributes
  let _diff = remote_text.diff(&remote_txn, YChange::identity);
}
