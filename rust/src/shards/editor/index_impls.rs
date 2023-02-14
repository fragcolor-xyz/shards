/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// Some parts taken from egui_node_graph
// MIT license
// Copyright (c) 2022 setzer22

use super::graph::*;
use super::id_types::*;

macro_rules! impl_index_traits {
  ($id_type:ty, $output_type:ty, $arena:ident) => {
    impl<A> std::ops::Index<$id_type> for Graph<A> {
      type Output = $output_type;

      fn index(&self, index: $id_type) -> &Self::Output {
        self.$arena.get(index).unwrap_or_else(|| {
          panic!(
            "{} index error for {:?}. Has the value been deleted?",
            stringify!($id_type),
            index
          )
        })
      }
    }

    impl<A> std::ops::IndexMut<$id_type> for Graph<A> {
      fn index_mut(&mut self, index: $id_type) -> &mut Self::Output {
        self.$arena.get_mut(index).unwrap_or_else(|| {
          panic!(
            "{} index error for {:?}. Has the value been deleted?",
            stringify!($id_type),
            index
          )
        })
      }
    }
  };
}

impl_index_traits!(NodeId, Node<A>, nodes);
