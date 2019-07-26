import nimline
import os

const
  modulePath = currentSourcePath().splitPath().head
cppincludes(modulePath & "")

emitc("/*INCLUDESECTION*/#define GB_STRING_IMPLEMENTATION")
emitc("/*INCLUDESECTION*/#include <cstdlib>")

type
  NativeGBString {.importcpp: "gbString", header: "gb_string.h".} = distinct cstring
  GbString* = object
    gbstr: NativeGBString

proc `$`*(s: GbString): string {.inline.} = $s.gbstr.cstring

proc `=destroy`*(s: var GbString) =
  when defined(testing): echo "=destroy called on: ", s
  if s.gbstr.cstring != nil:
    invokeFunction("gb_free_string", s.gbstr).to(void)
    s.gbstr = nil

proc `=`*(a: var GbString; b: GbString) =
  when defined(testing): echo "= called on: ", a
  if a.gbstr.pointer == b.gbstr.pointer: return
  `=destroy`(a)
  a.gbstr = invokeFunction("gb_duplicate_string", b.gbstr).to(NativeGBString)

proc `=move`*(a, b: var GbString) =
  when defined(testing): echo "=move called on: ", a
  if a.gbstr.pointer == b.gbstr.pointer: return
  `=destroy`(a)
  a.gbstr = b.gbstr

proc `=sink`*(a: var GbString; b: GbString) =
  when defined(testing): echo "=sink called on: ", a
  if a.gbstr.pointer == b.gbstr.pointer: return
  `=destroy`(a)
  a.gbstr = b.gbstr

converter toGbString*(nstr: string): GbString {.inline, noinit.} =
  result.gbstr = invokeFunction("gb_make_string_length", nstr.cstring, nstr.len).to(NativeGBString)

converter toGbString*(nstr: cstring): GbString {.inline, noinit.} =
  result.gbstr = invokeFunction("gb_make_string_length", nstr, nstr.len).to(NativeGBString)

converter asCString*(gstr: GbString): cstring {.inline, noinit.} = gstr.gbstr.cstring

proc len*(s: GbString): int {.inline.} =
  if s.gbstr.pointer == nil: return 0
  invokeFunction("gb_string_length", s.gbstr).to(int)

proc capacity*(s: GbString): int {.inline.} =
  if s.gbstr.pointer == nil: return 0
  invokeFunction("gb_string_capacity", s.gbstr).to(int)

proc clear*(s: var GbString) {.inline.} =
  if s.gbstr.pointer == nil: s.gbstr = invokeFunction("gb_make_string", "").to(NativeGBString)
  else: invokeFunction("gb_clear_string", s.gbstr).to(void)

proc setLen*(s: var GbString; len: int) {.inline.} =
  if s.gbstr.pointer == nil: s.gbstr = invokeFunction("gb_make_string", "").to(NativeGBString)
  s.gbstr = invokeFunction("gb_string_make_space_for", s.gbstr, len).to(NativeGBString)

proc `&=`*(a: var GbString, b: GbString) {.inline.} =
  if a.gbstr.pointer == nil: a.gbstr = invokeFunction("gb_make_string", "").to(NativeGBString)
  a.gbstr = invokeFunction("gb_append_string", a.gbstr, b.gbstr).to(NativeGBString)

proc `&`*(a: GbString, b: GbString): GbString {.inline.} =
  result.gbstr = invokeFunction("gb_duplicate_string", a.gbstr).to(NativeGBString)
  result.gbstr = invokeFunction("gb_append_string", result.gbstr, b.gbstr).to(NativeGBString)

template gb*(s: string): GbString = s.toGbString()

when isMainModule:
  var
    gs: GbString = "Hello world"
    g1: GbString = " and "
    g2: GbString = "Buonanotte"
  
  echo gs
  echo gs & g1 & g2
  gs &= g1
  gs &= g2
