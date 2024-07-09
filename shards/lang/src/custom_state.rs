/// CustomStateContainer provides a flexible and efficient way to store multiple custom states within AST nodes.
///
/// This approach was chosen to balance performance, flexibility, and simplicity.
///
/// Benefits:
/// - Efficient access to multiple custom states using a `HashMap`
/// - Simplifies state management by keeping related data close together
/// - Maintains low memory overhead through the use of `Box<dyn CustomAny>`
///
/// Considerations:
/// - Adds non-structural data to the AST nodes
/// - Requires careful handling during serialization/deserialization to avoid data loss
///
/// Alternative considered: External state storage with UUID references in AST nodes.
/// Current approach preferred due to reduced lookup overhead and simpler implementation.
use std::any::{Any, TypeId};
use std::collections::HashMap;
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

#[derive(Debug, Clone, Default)]
pub struct CustomStateContainer {
  states: HashMap<TypeId, Box<dyn CustomAny>>,
}

impl CustomStateContainer {
  pub fn new() -> Self {
    CustomStateContainer {
      states: HashMap::new(),
    }
  }

  pub fn set<T: 'static + CustomAny>(&mut self, state: T) {
    self.states.insert(TypeId::of::<T>(), Box::new(state));
  }

  pub fn get<T: 'static + CustomAny>(&self) -> Option<&T> {
    self
      .states
      .get(&TypeId::of::<T>())
      .and_then(|state| state.as_any().downcast_ref::<T>())
  }

  pub fn get_mut<T: 'static + CustomAny>(&mut self) -> Option<&mut T> {
    self
      .states
      .get_mut(&TypeId::of::<T>())
      .and_then(|state| state.as_any_mut().downcast_mut::<T>())
  }

  pub fn remove<T: 'static + CustomAny>(&mut self) {
    self.states.remove(&TypeId::of::<T>());
  }

  pub fn clear(&mut self) {
    self.states.clear();
  }

  pub fn get_or_insert_custom_state<T: 'static + CustomAny, F>(&mut self, default: F) -> &mut T
  where
    F: FnOnce() -> T,
  {
    use std::collections::hash_map::Entry;

    match self.states.entry(TypeId::of::<T>()) {
      Entry::Occupied(entry) => entry.into_mut().as_any_mut().downcast_mut::<T>().unwrap(),
      Entry::Vacant(entry) => entry
        .insert(Box::new(default()))
        .as_any_mut()
        .downcast_mut::<T>()
        .unwrap(),
    }
  }
}

impl PartialEq for CustomStateContainer {
  fn eq(&self, other: &Self) -> bool {
    self.states.len() == other.states.len()
      && self.states.iter().all(|(k, v)| {
        other
          .states
          .get(k)
          .map_or(false, |ov| v.eq_box(ov.as_ref()))
      })
  }
}

impl Clone for Box<dyn CustomAny> {
  fn clone(&self) -> Box<dyn CustomAny> {
    self.clone_box()
  }
}
