use lib0::any;
use lib0::any::Any;
use std::collections::HashMap;
use std::ffi::CStr;
use yrs::types::text::YChange;
use yrs::types::ToJson;
use yrs::updates::decoder::Decode;
use yrs::*;

use crate::shardsc::*;

use crate::types::{Table, Var};

impl Into<Any> for Var {
  fn into(self) -> Any {
    unsafe {
      match self.valueType {
        SHType_None => Any::Null,
        SHType_Bool => Any::Bool(self.payload.__bindgen_anon_1.boolValue),
        SHType_Int => Any::BigInt(self.payload.__bindgen_anon_1.intValue),
        SHType_Float => Any::Number(self.payload.__bindgen_anon_1.floatValue),
        SHType_Bytes => {
          let s = std::slice::from_raw_parts(
            self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesValue,
            self.payload.__bindgen_anon_1.__bindgen_anon_4.bytesSize as usize,
          );
          Any::Buffer(Box::from(s))
        }
        SHType_String => {
          let s = CStr::from_ptr(self.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue)
            .to_str()
            .unwrap();
          Any::String(Box::from(s))
        }
        SHType_Seq => {
          let v = self.payload.__bindgen_anon_1.seqValue;
          let mut arr = Vec::with_capacity(v.len as usize);
          for i in 0..v.len {
            arr.push((*v.elements.offset(i as isize)).into());
          }
          Any::Array(arr.into_boxed_slice())
        }
        SHType_Table => {
          let t = self.payload.__bindgen_anon_1.tableValue;
          let tab: Table = Table::try_from(t).expect("Failed to convert table");
          let mut map = HashMap::new();
          for (k, v) in tab.iter() {
            let k = CStr::from_ptr(k.0).to_str().unwrap().to_string();
            map.insert(k, v.into());
          }
          Any::Map(Box::new(map))
        }
        _ => Any::Undefined,
      }
    }
  }
}

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

pub fn sample4() {
  let doc = Doc::new();
  let map = doc.get_or_insert_map("map");
  let mut txn = doc.transact_mut();

  // insert value
  let v1: Var = 1.0.into();
  map.insert(&mut txn, "key1", v1);

  let mut t: Table = Table::new();
  let v2: Var = 2.0.into();
  t.insert_fast_static("k\0", &v2);
  let tv: Var = (&t).into();

  // insert nested shared type
  map.insert(&mut txn, "key2", tv);

  assert_eq!(
    map.to_json(&txn),
    any!({
      "key1": 1,
      "key2": {
        "k": 2,
      }
    })
  );

  // get value
  assert_eq!(map.get(&txn, "key1"), Some(1.0.into()));

  // remove entry
  map.remove(&mut txn, "key1");
  assert_eq!(map.get(&txn, "key1"), None);
}
