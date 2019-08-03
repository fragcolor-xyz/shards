# Package

version       = "0.1.0"
author        = "Giovanni"
description   = "A new awesome nimble package"
license       = "Proprietary"
backend       = "cpp"
installDirs   = @["blocks"]
installFiles  = @["types.nim", "ops.nim", "images.nim"]

# Dependencies

requires "nim >= 0.20.9"
requires "nimline#head"
requires "fragments#head"
