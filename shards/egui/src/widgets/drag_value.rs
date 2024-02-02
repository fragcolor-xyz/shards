use egui::{emath, DragValue, Response, Ui, Widget};

pub struct CustomDragValue<'a> {
  pub drag_value: DragValue<'a>,
}

pub fn adjust_drag_value<'a, Num: emath::Numeric>(
  value: &Num,
  drag_value: DragValue<'a>,
) -> DragValue<'a> {
  let val = f64::abs(value.to_f64());
  let log = f64::log(f64::max(0.0, val - 10.0), 10.0);
  let speed = f64::clamp(log * 0.1, 0.01, 10.0);
  drag_value.update_while_editing(false).speed(speed)
}

impl<'a> CustomDragValue<'a> {
  pub fn new<Num: emath::Numeric>(value: &'a mut Num) -> Self {
    let num_copy = *value;
    let mut drag_value = DragValue::new(value);
    drag_value = adjust_drag_value(&num_copy, drag_value);
    Self { drag_value }
  }

  pub fn from_egui<Num: emath::Numeric>(value: &Num, drag_value: DragValue<'a>) -> Self {
    Self {
      drag_value: adjust_drag_value(value, drag_value),
    }
  }
}

impl<'a> Widget for CustomDragValue<'a> {
  fn ui(self, ui: &mut Ui) -> Response {
    self.drag_value.ui(ui)
  }
}
