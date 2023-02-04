---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Debugging

For debugging from within vscode using this toolchain, you should use the [Microsoft C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extension.

Here is and example of a launch configuration:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(Windows) Launch",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${command:cmake.launchTargetPath}",
            "args": [
                "src/tests/general.edn",
            ],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
        }
    ]
}
```

--8<-- "includes/license.md"
