type
  Image*[T] = object
    width*: int
    height*: int
    channels*: int
    data*: ptr UncheckedArray[T]

  Indexable*[T] = concept x
    x[int] is T
    x[int] = T

proc asImage*[T](a: var openarray[T], width, height, channels: int): Image[T] {.inline, noinit.} =
  assert(a.len == (width * height * channels), $a.len)
  result.width = width
  result.height = height
  result.channels = channels
  result.data = cast[ptr UncheckedArray[T]](addr a[0])

proc `[]`*[T](v: Image[T]; index: int): T {.inline, noinit.} = v.data[index]

proc `[]=`*[T](v: Image[T]; index: int; value: T) {.inline.} = v.data[index] = value

proc getPixel*[T](data: Image[T]; x, y: int; output: var Indexable[T], fromChannel, toChannel: int) {.inline.} =
  for d in fromChannel..toChannel:
    let address = ((data.width * y) + x) * data.channels
    output[d] = data[address + d]

proc getPixel*[T](data: Image[T]; ix, iy, ox, oy: int; output: var Indexable[T], fromChannel, toChannel: int) {.inline.} =
  var
    x = ix
    y = iy
    xox = x + ox
    yoy = y + oy

  # Padding by shifting to nearest pixel
  if xox >= 0 and xox < data.width:
    x += ox
  
  if yoy >= 0 and yoy < data.height:
    y += oy

  data.getPixel(x, y, output, fromChannel, toChannel)

proc setPixel*[T](data: Image[T]; x, y: int; input: var openarray[T], fromChannel, toChannel: int) {.inline.} =
  for d in fromChannel..toChannel:
    let address = ((data.width * y) + x) * data.channels
    data[address + d] = input[d]

proc setPixel*[T](data: Image[T]; ix, iy, ox, oy: int; input: var openarray[T], fromChannel, toChannel: int) {.inline.} =
  var
    x = ix
    y = iy
    xox = x + ox
    yoy = y + oy

  # Padding by shifting to nearest pixel
  if xox >= 0 and xox < data.width:
    x += ox
  
  if yoy >= 0 and yoy < data.height:
    y += oy

  data.setPixel(x, y, input, fromChannel, toChannel)

proc getPixels3x3*[T](texture: Image[T]; ix, iy: int; output: var openarray[T]; fromChannelIndex, toChannelIndex: int = -1) {.inline.} =
  var
    index = 0
    cFrom = if fromChannelIndex != -1: fromChannelIndex else: 0
    cTo = if toChannelIndex != -1: toChannelIndex else: texture.channels - 1
    channels = (cTo + 1 - cFrom)

  assert(output.len == 3 * 3 * channels, $(output.len) & " instead expected: " & $(3 * 3 * channels))
  
  for y in countdown(1, -1):
    for x in countdown(1, -1):
      var uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
      getPixel(texture, ix, iy,  x,  y, uncheckedPart, cFrom, cTo)
      inc index
  
  assert(index == output.len div channels, $index)

proc getPixels3x3*[T](texture: Image[T]; ix, iy: int; output: ptr UncheckedArray[T]; fromChannelIndex, toChannelIndex: int = -1) {.inline.} =
  var
    index = 0
    cFrom = if fromChannelIndex != -1: fromChannelIndex else: 0
    cTo = if toChannelIndex != -1: toChannelIndex else: texture.channels - 1
    channels = (cTo + 1 - cFrom)

  for y in countdown(1, -1):
    for x in countdown(1, -1):
      var uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
      getPixel(texture, ix, iy,  x,  y, uncheckedPart, cFrom, cTo)
      inc index
  
proc getPixelsFrag3x3*[T](texture: Image[T]; ix, iy: int; output: var openarray[T]; fromChannelIndex, toChannelIndex: int = -1) {.inline.} =
  var
    index = 0
    cFrom = if fromChannelIndex != -1: fromChannelIndex else: 0
    cTo = if toChannelIndex != -1: toChannelIndex else: texture.channels - 1
    channels = (cTo + 1 - cFrom)
    uncheckedPart: ptr UncheckedArray[T]

  assert(output.len == 3 * 3 * channels, $(output.len) & " instead expected: " & $(3 * 3 * channels))
  
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1,  1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0,  1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1,  1, uncheckedPart, cFrom, cTo)
  inc index
  
  assert(index == output.len div channels, $index)

proc getPixelsFrag3x3*[T](texture: Image[T]; ix, iy: int; output: ptr UncheckedArray[T]; fromChannelIndex, toChannelIndex: int = -1) {.inline.} =
  var
    index = 0
    cFrom = if fromChannelIndex != -1: fromChannelIndex else: 0
    cTo = if toChannelIndex != -1: toChannelIndex else: texture.channels - 1
    channels = (cTo + 1 - cFrom)
    uncheckedPart: ptr UncheckedArray[T]
  
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1, -1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1,  0, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy, -1,  1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  0,  1, uncheckedPart, cFrom, cTo)
  inc index
  uncheckedPart = cast[ptr UncheckedArray[T]](addr output[index * channels])
  getPixel(texture, ix, iy,  1,  1, uncheckedPart, cFrom, cTo)
  inc index