/// CustomStateContainer provides a flexible and efficient way to store multiple custom states within AST nodes.
///
/// This approach was chosen to balance performance, flexibility, and simplicity.
///
/// Benefits:
/// - Efficient access to multiple custom states using a `DashMap`
/// - Simplifies state management by keeping related data close together
/// - Maintains low memory overhead through the use of `Box<dyn CustomAny>`
///
/// Considerations:
/// - Adds non-structural data to the AST nodes
/// - Requires careful handling during serialization/deserialization to avoid data loss
///
/// Alternative considered: External state storage with UUID references in AST nodes.
/// Current approach preferred due to reduced lookup overhead and simpler implementation.
use dashmap::DashMap;
use once_cell::sync::OnceCell;
use std::any::{Any, TypeId};
use std::fmt::Debug;

pub trait CustomAny: Any + Debug {
  fn as_any(&self) -> &dyn Any;
  fn as_any_mut(&mut self) -> &mut dyn Any;
  fn clone_box(&self) -> Box<dyn CustomAny>;
  fn eq_box(&self, other: &dyn CustomAny) -> bool;
}

impl<T: 'static + Any + Debug + Clone + PartialEq> CustomAny for T {
  fn as_any(&self) -> &dyn Any {
    self
  }

  fn as_any_mut(&mut self) -> &mut dyn Any {
    self
  }

  fn clone_box(&self) -> Box<dyn CustomAny> {
    Box::new(self.clone())
  }

  fn eq_box(&self, other: &dyn CustomAny) -> bool {
    other
      .as_any()
      .downcast_ref::<T>()
      .map_or(false, |a| self == a)
  }
}

#[derive(Debug, Default)]
pub struct CustomStateContainer {
  states: OnceCell<DashMap<TypeId, Box<dyn CustomAny>>>,
}

impl CustomStateContainer {
  pub fn new() -> Self {
    CustomStateContainer {
      states: OnceCell::new(),
    }
  }

  fn initialize_states(&self) -> &DashMap<TypeId, Box<dyn CustomAny>> {
    self.states.get_or_init(DashMap::new)
  }

  pub fn set<T: 'static + CustomAny>(&self, state: T) {
    self
      .initialize_states()
      .insert(TypeId::of::<T>(), Box::new(state));
  }

  pub fn with<T, F, R>(&self, f: F) -> Option<R>
  where
    T: 'static + CustomAny,
    F: FnOnce(&T) -> R,
  {
    self.states.get().and_then(|states| {
      states
        .get(&TypeId::of::<T>())
        .and_then(|state| state.as_any().downcast_ref::<T>().map(f))
    })
  }

  pub fn with_mut<T, F, R>(&self, f: F) -> Option<R>
  where
    T: 'static + CustomAny,
    F: FnOnce(&mut T) -> R,
  {
    self.states.get().and_then(|states| {
      states
        .get_mut(&TypeId::of::<T>())
        .and_then(|mut state| state.as_any_mut().downcast_mut::<T>().map(f))
    })
  }

  pub fn with_or_insert_with<T, F, R>(&self, default: impl FnOnce() -> T, f: F) -> R
  where
    T: 'static + CustomAny,
    F: FnOnce(&mut T) -> R,
  {
    self.initialize_states();
    let type_id = TypeId::of::<T>();
    let states = self.states.get().unwrap();

    if let Some(mut state) = states.get_mut(&type_id) {
      f(state.as_any_mut().downcast_mut::<T>().unwrap())
    } else {
      let mut new_state = default();
      let result = f(&mut new_state);
      states.insert(type_id, Box::new(new_state));
      result
    }
  }

  pub fn remove<T: 'static + CustomAny>(&self) {
    if let Some(states) = self.states.get() {
      states.remove(&TypeId::of::<T>());
    }
  }

  pub fn clear(&self) {
    if let Some(states) = self.states.get() {
      states.clear();
    }
  }
}

impl Clone for CustomStateContainer {
  fn clone(&self) -> Self {
    let cloned_states: Vec<(TypeId, Box<dyn CustomAny>)> =
      self.states.get().map_or(Vec::new(), |states| {
        states
          .iter()
          .map(|entry| (entry.key().clone(), entry.value().clone_box()))
          .collect()
      });
    CustomStateContainer {
      states: OnceCell::with_value(DashMap::from_iter(cloned_states)),
    }
  }
}

impl PartialEq for CustomStateContainer {
  fn eq(&self, other: &Self) -> bool {
    let self_states = self.states.get();
    let other_states = other.states.get();

    let self_states_vec: Vec<_> = self_states.map_or(Vec::new(), |s| s.iter().collect());
    let other_states_vec: Vec<_> = other_states.map_or(Vec::new(), |s| s.iter().collect());

    self_states_vec.len() == other_states_vec.len()
      && self_states_vec.iter().all(|self_entry| {
        other_states_vec
          .iter()
          .find(|other_entry| self_entry.key() == other_entry.key())
          .map_or(false, |other_entry| {
            self_entry.value().eq_box(other_entry.value().as_ref())
          })
      })
  }
}

impl Clone for Box<dyn CustomAny> {
  fn clone(&self) -> Box<dyn CustomAny> {
    self.clone_box()
  }
}
