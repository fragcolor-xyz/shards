use super::*;
use egui::ahash::HashMapExt;
use egui::CursorIcon;
use egui::Event;
use egui::Modifiers;
use egui::TouchDeviceId;
use egui::TouchId;
use egui::ViewportId;
use egui::ViewportInfo;
use shards::util::from_raw_parts_allow_null;
use std::ffi::CStr;

const SCROLL_SPEED: f32 = 50.0;

fn to_egui_pointer_button(btn: egui_PointerButton) -> egui::PointerButton {
  match btn {
    egui_PointerButton_Primary => egui::PointerButton::Primary,
    egui_PointerButton_Secondary => egui::PointerButton::Secondary,
    egui_PointerButton_Middle => egui::PointerButton::Middle,
    _ => panic!("Invalid enum value"),
  }
}

fn to_egui_touch_phase(btn: egui_TouchPhase) -> egui::TouchPhase {
  match btn {
    egui_TouchPhase_Cancel => egui::TouchPhase::Cancel,
    egui_TouchPhase_End => egui::TouchPhase::End,
    egui_TouchPhase_Move => egui::TouchPhase::Move,
    egui_TouchPhase_Start => egui::TouchPhase::Start,
    _ => panic!("Invalid enum value"),
  }
}

impl From<egui_ModifierKeys> for egui::Modifiers {
  fn from(mk: egui_ModifierKeys) -> Self {
    Self {
      alt: mk.alt,
      ctrl: mk.ctrl,
      shift: mk.shift,
      mac_cmd: mk.macCmd,
      command: mk.command,
    }
  }
}

fn sdl_to_egui_key(key_code: SDL_Keycode) -> Result<egui::Key, ()> {
  match key_code {
    SDLK_RETURN => Ok(egui::Key::Enter),
    SDLK_ESCAPE => Ok(egui::Key::Escape),
    SDLK_BACKSPACE => Ok(egui::Key::Backspace),
    SDLK_TAB => Ok(egui::Key::Tab),
    SDLK_SPACE => Ok(egui::Key::Space),
    SDLK_0 => Ok(egui::Key::Num0),
    SDLK_1 => Ok(egui::Key::Num1),
    SDLK_2 => Ok(egui::Key::Num2),
    SDLK_3 => Ok(egui::Key::Num3),
    SDLK_4 => Ok(egui::Key::Num4),
    SDLK_5 => Ok(egui::Key::Num5),
    SDLK_6 => Ok(egui::Key::Num6),
    SDLK_7 => Ok(egui::Key::Num7),
    SDLK_8 => Ok(egui::Key::Num8),
    SDLK_9 => Ok(egui::Key::Num9),
    SDLK_a => Ok(egui::Key::A),
    SDLK_b => Ok(egui::Key::B),
    SDLK_c => Ok(egui::Key::C),
    SDLK_d => Ok(egui::Key::D),
    SDLK_e => Ok(egui::Key::E),
    SDLK_f => Ok(egui::Key::F),
    SDLK_g => Ok(egui::Key::G),
    SDLK_h => Ok(egui::Key::H),
    SDLK_i => Ok(egui::Key::I),
    SDLK_j => Ok(egui::Key::J),
    SDLK_k => Ok(egui::Key::K),
    SDLK_l => Ok(egui::Key::L),
    SDLK_m => Ok(egui::Key::M),
    SDLK_n => Ok(egui::Key::N),
    SDLK_o => Ok(egui::Key::O),
    SDLK_p => Ok(egui::Key::P),
    SDLK_q => Ok(egui::Key::Q),
    SDLK_r => Ok(egui::Key::R),
    SDLK_s => Ok(egui::Key::S),
    SDLK_t => Ok(egui::Key::T),
    SDLK_u => Ok(egui::Key::U),
    SDLK_v => Ok(egui::Key::V),
    SDLK_w => Ok(egui::Key::W),
    SDLK_x => Ok(egui::Key::X),
    SDLK_y => Ok(egui::Key::Y),
    SDLK_z => Ok(egui::Key::Z),
    SDLK_INSERT => Ok(egui::Key::Insert),
    SDLK_HOME => Ok(egui::Key::Home),
    SDLK_PAGEUP => Ok(egui::Key::PageUp),
    SDLK_DELETE => Ok(egui::Key::Delete),
    SDLK_END => Ok(egui::Key::End),
    SDLK_PAGEDOWN => Ok(egui::Key::PageDown),
    SDLK_RIGHT => Ok(egui::Key::ArrowRight),
    SDLK_LEFT => Ok(egui::Key::ArrowLeft),
    SDLK_DOWN => Ok(egui::Key::ArrowDown),
    SDLK_UP => Ok(egui::Key::ArrowUp),
    SDLK_KP_ENTER => Ok(egui::Key::Enter),
    SDLK_KP_1 => Ok(egui::Key::Num1),
    SDLK_KP_2 => Ok(egui::Key::Num2),
    SDLK_KP_3 => Ok(egui::Key::Num3),
    SDLK_KP_4 => Ok(egui::Key::Num4),
    SDLK_KP_5 => Ok(egui::Key::Num5),
    SDLK_KP_6 => Ok(egui::Key::Num6),
    SDLK_KP_7 => Ok(egui::Key::Num7),
    SDLK_KP_8 => Ok(egui::Key::Num8),
    SDLK_KP_9 => Ok(egui::Key::Num9),
    SDLK_KP_0 => Ok(egui::Key::Num0),
    _ => Err(()),
  }
}

pub fn to_egui_cursor_icon(i: CursorIcon) -> egui_CursorIcon {
  match i {
    CursorIcon::Default => egui_CursorIcon_Default,
    CursorIcon::None => egui_CursorIcon_None,
    CursorIcon::ContextMenu => egui_CursorIcon_ContextMenu,
    CursorIcon::Help => egui_CursorIcon_Help,
    CursorIcon::PointingHand => egui_CursorIcon_PointingHand,
    CursorIcon::Progress => egui_CursorIcon_Progress,
    CursorIcon::Wait => egui_CursorIcon_Wait,
    CursorIcon::Cell => egui_CursorIcon_Cell,
    CursorIcon::Crosshair => egui_CursorIcon_Crosshair,
    CursorIcon::Text => egui_CursorIcon_Text,
    CursorIcon::VerticalText => egui_CursorIcon_VerticalText,
    CursorIcon::Alias => egui_CursorIcon_Alias,
    CursorIcon::Copy => egui_CursorIcon_Copy,
    CursorIcon::Move => egui_CursorIcon_Move,
    CursorIcon::NoDrop => egui_CursorIcon_NoDrop,
    CursorIcon::NotAllowed => egui_CursorIcon_NotAllowed,
    CursorIcon::Grab => egui_CursorIcon_Grab,
    CursorIcon::Grabbing => egui_CursorIcon_Grabbing,
    CursorIcon::AllScroll => egui_CursorIcon_AllScroll,
    CursorIcon::ResizeHorizontal => egui_CursorIcon_ResizeHorizontal,
    CursorIcon::ResizeNeSw => egui_CursorIcon_ResizeNeSw,
    CursorIcon::ResizeNwSe => egui_CursorIcon_ResizeNwSe,
    CursorIcon::ResizeVertical => egui_CursorIcon_ResizeVertical,
    CursorIcon::ResizeEast => egui_CursorIcon_ResizeEast,
    CursorIcon::ResizeSouthEast => egui_CursorIcon_ResizeSouthEast,
    CursorIcon::ResizeSouth => egui_CursorIcon_ResizeSouth,
    CursorIcon::ResizeSouthWest => egui_CursorIcon_ResizeSouthWest,
    CursorIcon::ResizeWest => egui_CursorIcon_ResizeWest,
    CursorIcon::ResizeNorthWest => egui_CursorIcon_ResizeNorthWest,
    CursorIcon::ResizeNorth => egui_CursorIcon_ResizeNorth,
    CursorIcon::ResizeNorthEast => egui_CursorIcon_ResizeNorthEast,
    CursorIcon::ResizeColumn => egui_CursorIcon_ResizeColumn,
    CursorIcon::ResizeRow => egui_CursorIcon_ResizeRow,
    CursorIcon::ZoomIn => egui_CursorIcon_ZoomIn,
    CursorIcon::ZoomOut => egui_CursorIcon_ZoomOut,
  }
}

#[derive(Debug)]
pub enum TranslationError {
  Text(std::str::Utf8Error),
}

impl From<std::str::Utf8Error> for TranslationError {
  fn from(e: std::str::Utf8Error) -> Self {
    TranslationError::Text(e)
  }
}

impl std::fmt::Display for TranslationError {
  fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
    match self {
      TranslationError::Text(text) => text.fmt(f),
    }
  }
}

pub fn translate_raw_input(input: &egui_Input) -> Result<egui::RawInput, TranslationError> {
  let mut events = Vec::new();

  unsafe {
    let in_events = from_raw_parts_allow_null(input.inputEvents, input.numInputEvents);
    for in_event in in_events {
      let out_event = match in_event.common.type_ {
        egui_InputEventType_PointerMoved => {
          let event = &in_event.pointerMoved;
          Some(Event::PointerMoved(event.pos.into()))
        }
        egui_InputEventType_PointerButton => {
          let event = &in_event.pointerButton;
          Some(Event::PointerButton {
            pos: event.pos.into(),
            button: to_egui_pointer_button(event.button),
            pressed: event.pressed,
            modifiers: event.modifiers.into(),
          })
        }
        egui_InputEventType_Scroll => {
          let event = &in_event.scroll;
          let virtual_delta_x = event.delta.x * SCROLL_SPEED;
          let virtual_delta_y = event.delta.y * SCROLL_SPEED;
          Some(Event::Scroll(egui::vec2(virtual_delta_x, virtual_delta_y)))
        }
        egui_InputEventType_Text => {
          let event = &in_event.text;
          let text = CStr::from_ptr(event.text).to_str()?.to_owned();
          Some(Event::Text(text))
        }
        egui_InputEventType_Key => {
          let event = &in_event.key;
          match sdl_to_egui_key(event.key) {
            Ok(key) => Some(Event::Key {
              key,
              pressed: event.pressed,
              repeat: event.repeat,
              modifiers: event.modifiers.into(),
              physical_key: None,
            }),
            Err(_) => None,
          }
        }
        egui_InputEventType_CompositionStart => Some(Event::CompositionStart),
        egui_InputEventType_CompositionUpdate => {
          let event = &in_event.compositionUpdate;
          let text = CStr::from_ptr(event.text).to_str()?.to_owned();
          Some(Event::CompositionUpdate(text))
        }
        egui_InputEventType_CompositionEnd => {
          let event = &in_event.compositionEnd;
          let text = CStr::from_ptr(event.text).to_str()?.to_owned();
          Some(Event::CompositionEnd(text))
        }
        egui_InputEventType_Copy => Some(Event::Copy),
        egui_InputEventType_Cut => Some(Event::Cut),
        egui_InputEventType_Paste => {
          let event = &in_event.paste;
          let str = CStr::from_ptr(event.str_).to_str()?.to_owned();
          Some(Event::Paste(str))
        }
        egui_InputEventType_PointerGone => Some(Event::PointerGone),
        egui_InputEventType_Touch => {
          let event = &in_event.touch;
          Some(Event::Touch {
            device_id: TouchDeviceId(event.deviceId.id),
            id: TouchId(event.id.id),
            phase: to_egui_touch_phase(event.phase),
            pos: event.pos.into(),
            force: Some(event.force),
          })
        }
        _ => {
          panic!("Invalid input event type")
        }
      };

      if let Some(event) = out_event {
        events.push(event);
      }
    }
  }

  Ok(egui::RawInput {
    dropped_files: Vec::new(),
    hovered_files: Vec::new(),
    events,
    modifiers: Modifiers::default(),
    time: Some(input.time),
    predicted_dt: input.predictedDeltaTime,
    screen_rect: Some(input.screenRect.into()),
    max_texture_side: None,
    focused: true, // FIXME
    viewport_id: ViewportId::default(),
    viewports: {
      let info = ViewportInfo {
        native_pixels_per_point: Some(input.pixelsPerPoint),
        ..Default::default()
      };
      let mut map = egui::ViewportIdMap::new();
      map.insert(ViewportId::default(), info);
      map
    },
  })
}

pub struct InputTranslator {
  egui_translator: *mut gfx_EguiInputTranslator,
}

impl Default for InputTranslator {
  fn default() -> Self {
    Self::new()
  }
}

impl Drop for InputTranslator {
  fn drop(&mut self) {
    unsafe {
      gfx_EguiInputTranslator_destroy(self.egui_translator);
    }
  }
}

unsafe impl Send for InputTranslator {}
unsafe impl Sync for InputTranslator {}

impl InputTranslator {
  pub fn new() -> Self {
    unsafe {
      Self {
        egui_translator: gfx_EguiInputTranslator_create(),
      }
    }
  }

  pub fn as_mut_ptr(&self) -> *mut gfx_EguiInputTranslator {
    self.egui_translator
  }
}
