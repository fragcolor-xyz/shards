---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Code completion

For code completion and reference inspection, use the [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) extension for vscode.

It needs to be pointed to the build folder where you configured the CMake project, do this by creating a `.clangd` file in the project root

The file should look something like this

```
CompileFlags:
    CompilationDatabase: build/clang-15.0.7-x86_64-pc-windows-msvc/Debug
```

Replace the path with the folder where your configured the project. To know for sure if you specified the correct folder, the folder should contain a `compile_commands.json` file.

--8<-- "includes/license.md"
