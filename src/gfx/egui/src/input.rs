use super::*;
use egui::Event;
use egui::Modifiers;
use egui::Vec2;
use std::slice::from_raw_parts;

const SCROLL_SPEED: f32 = 50.0;

fn to_egui_pointer_button(btn: egui_PointerButton) -> egui::PointerButton {
    match (btn) {
        egui_PointerButton_Primary => egui::PointerButton::Primary,
        egui_PointerButton_Secondary => egui::PointerButton::Secondary,
        egui_PointerButton_Middle => egui::PointerButton::Middle,
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

pub fn translate_raw_input(input: &egui_Input) -> egui::RawInput {
    let mut events = Vec::new();

    unsafe {
        let in_events = from_raw_parts(input.inputEvents, input.numInputEvents);
        for in_event in in_events {
            let out_event = match in_event.common.type_ {
                egui_InputEventType_PointerMoved => {
                    let event = &in_event.pointerMoved;
                    Event::PointerMoved(event.pos.into())
                }
                egui_InputEventType_PointerButton => {
                    let event = &in_event.pointerButton;
                    Event::PointerButton {
                        pos: event.pos.into(),
                        button: to_egui_pointer_button(event.button),
                        pressed: event.pressed,
                        modifiers: event.modifiers.into(),
                    }
                }
                egui_InputEventType_Scroll => {
                    let event = &in_event.scroll;
                    let virtual_delta_x = event.delta.x * SCROLL_SPEED;
                    let virtual_delta_y = event.delta.y * SCROLL_SPEED;
                    Event::Scroll(egui::vec2(virtual_delta_x, virtual_delta_y))
                }
                _ => {
                    panic!("Invalid input event type")
                }
            };
            events.push(out_event);
        }
    }

    egui::RawInput {
        dropped_files: Vec::new(),
        hovered_files: Vec::new(),
        events: events,
        modifiers: Modifiers::default(),
        time: Some(input.time),
        predicted_dt: input.predictedDeltaTime,
        screen_rect: Some(input.screenRect.into()),
        pixels_per_point: Some(input.pixelsPerPoint),
        max_texture_side: None,
    }
}
