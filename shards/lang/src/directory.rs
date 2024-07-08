use crate::{
  ast::{Rule, ShardsParser},
  eval, include_shards, read,
};
use once_cell::sync::OnceCell;
use pest::Parser;
use shards::{
  shlog_debug,
  types::{AutoTableVar, ClonedVar, Mesh},
};
use std::{
  collections::BTreeSet,
  sync::{atomic::AtomicBool, mpsc, Arc},
  thread,
};

static GLOBAL_MAP: OnceCell<AutoTableVar> = OnceCell::new();

pub fn new_cancellation_token() -> Arc<AtomicBool> {
  Arc::new(AtomicBool::new(false))
}

pub fn get_global_map() -> &'static AutoTableVar {
  GLOBAL_MAP.get_or_init(|| {
    let mut directory = AutoTableVar::new();
    let mut result = include_shards!("directory.shs");
    assert!(result.0.is_table());
    // swap the table into the directory
    std::mem::swap(&mut directory.0 .0, &mut result.0);
    directory
  })
}

static GLOBAL_NAME_BTREE: OnceCell<BTreeSet<String>> = OnceCell::new();

pub fn get_global_name_btree() -> &'static BTreeSet<String> {
  GLOBAL_NAME_BTREE.get_or_init(|| {
    let mut name_btree = BTreeSet::new();
    let map = get_global_map();
    let shards = map.0.get_fast_static("shards").as_table().unwrap();
    for (key, _) in shards.iter() {
      let name: &str = key.as_ref().try_into().unwrap();
      name_btree.insert(name.to_owned());
    }
    name_btree
  })
}

static GLOBAL_DB_QUERY_CHANNEL_SENDER: OnceCell<
  mpsc::Sender<(ClonedVar, mpsc::Sender<ClonedVar>)>,
> = OnceCell::new();

pub fn get_global_visual_shs_channel_sender(
) -> &'static mpsc::Sender<(ClonedVar, mpsc::Sender<ClonedVar>)> {
  GLOBAL_DB_QUERY_CHANNEL_SENDER.get_or_init(|| {
    let (sender, receiver): (
      mpsc::Sender<(ClonedVar, mpsc::Sender<ClonedVar>)>,
      mpsc::Receiver<(ClonedVar, mpsc::Sender<ClonedVar>)>,
    ) = mpsc::channel();
    thread::Builder::new()
      .name("global_visual_shs_thread".to_string())
      .spawn(move || {
        loop {
          if let Ok((query, response_sender)) = receiver.recv() {
            // Process the query and get the result
            let result: ClonedVar = process_query(query);
            // Send the result back through the response sender
            if response_sender.send(result).is_err() {
              shlog_debug!("global_visual_shs: response sender failed.");
            }
          } else {
            shlog_debug!("global_visual_shs: receiver ending.");
            break;
          }
        }
      })
      .expect("Failed to spawn global_visual_shs_thread");
    sender
  })
}

fn process_query(_query: ClonedVar) -> ClonedVar {
  ClonedVar::default()
}
