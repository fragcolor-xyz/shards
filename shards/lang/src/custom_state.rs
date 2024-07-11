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
use std::cell::RefCell;
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

#[derive(Debug, Default)]
pub struct CustomStateContainer {
  states: RefCell<HashMap<TypeId, Box<dyn CustomAny>>>,
}

impl CustomStateContainer {
  pub fn new() -> Self {
    CustomStateContainer {
      states: RefCell::new(HashMap::new()),
    }
  }

  pub fn set<T: 'static + CustomAny>(&self, state: T) {
    self
      .states
      .borrow_mut()
      .insert(TypeId::of::<T>(), Box::new(state));
  }

  pub fn get<T: 'static + CustomAny>(&self) -> Option<T>
  where
    T: Clone,
  {
    self
      .states
      .borrow()
      .get(&TypeId::of::<T>())
      .and_then(|state| state.as_any().downcast_ref::<T>())
      .cloned()
  }

  pub fn with<T, F, R>(&self, f: F) -> Option<R>
  where
    T: 'static + CustomAny,
    F: FnOnce(&T) -> R,
  {
    self
      .states
      .borrow()
      .get(&TypeId::of::<T>())
      .and_then(|state| state.as_any().downcast_ref::<T>())
      .map(f)
  }

  pub fn with_mut<T, F, R>(&self, f: F) -> Option<R>
  where
    T: 'static + CustomAny,
    F: FnOnce(&mut T) -> R,
  {
    self
      .states
      .borrow_mut()
      .get_mut(&TypeId::of::<T>())
      .and_then(|state| state.as_any_mut().downcast_mut::<T>())
      .map(f)
  }

  pub fn remove<T: 'static + CustomAny>(&self) {
    self.states.borrow_mut().remove(&TypeId::of::<T>());
  }

  pub fn clear(&self) {
    self.states.borrow_mut().clear();
  }

  pub fn get_or_insert_with<T, F>(&self, default: F) -> T
  where
    T: 'static + CustomAny + Clone,
    F: FnOnce() -> T,
  {
    let type_id = TypeId::of::<T>();
    let mut states = self.states.borrow_mut();

    if !states.contains_key(&type_id) {
      let new_state = default();
      states.insert(type_id, Box::new(new_state.clone()));
      new_state
    } else {
      states
        .get(&type_id)
        .and_then(|state| state.as_any().downcast_ref::<T>())
        .unwrap()
        .clone()
    }
  }
}

impl Clone for CustomStateContainer {
  fn clone(&self) -> Self {
    CustomStateContainer {
      states: RefCell::new(self.states.borrow().clone()),
    }
  }
}

impl PartialEq for CustomStateContainer {
  fn eq(&self, other: &Self) -> bool {
    let self_states = self.states.borrow();
    let other_states = other.states.borrow();

    self_states.len() == other_states.len()
      && self_states.iter().all(|(k, v)| {
        other_states
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
