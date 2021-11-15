# Chainblocks embedded content

## Building content & headers

- Build bx (bin2c) and copy the tools to the `bx/tools/<os>` folder (remove Release/Debug suffix)
- Build bgfx (shaderc) and copy the tools to the `bgfx/tools/<os>` folder (remove Release/Debug suffix)
- Run `BGFX=<path to bgfx> make rebuild` in this folder or a specific subfolder

> bin2c must build from scratch, the bundled bin2c generates corrupt source files!
