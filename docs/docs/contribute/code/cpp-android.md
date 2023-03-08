---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# C++ Setup (android)

This page describes the setup required to build for android

## Required software

- [Android SDK](https://developer.android.com/studio)
- [OpenJDK 17](https://jdk.java.net/archive/) or [Oracle JDK 17](https://www.oracle.com/java/technologies/downloads/#java17) (Other versions will most likely not work)

## Installing the android SDK

Install the android SDK normally using android studio or the command line tools from [developer.android.com/studio](https://developer.android.com/studio)

## Install the Java JDK

Install the Oracle JDK or OpenJDK from your package manager or from the official website

## Install the required SDK components

The following components are required to be intalled with the Android SDK manager or from Android Studio:

- `build-tools;33.0.2`       (version 33.0.2)
- `ndk;25.2.9519653`         (version 25.2.9519653)
- `platform-tools`           (version 34.0.0)
- `platforms;android-33`     (version 2)

The given versions are tested to work, you can try different versions of any of these tools at your own risk.

## Telling CMake about the compiler

### Using the vscode CMake Tools extension

Using [this](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension for vscode, you can tell it which compiler to use by editing the toolchains file.

Inside vscode, open the command menu (CTRL+SHIFT+P) and run the command `CMake: Edit User-Local CMake Kits`, this will open the kits json file.

Inside this file, add an entry for the newly installed compiler, it should look something like this.
You can leave the existing entries or remove them if you don't use them.

```json
[
  {
    "name": "android-25.2.9519653-arm64-android33",
    "toolchainFile": "C://Android//ndk//25.2.9519653//build//cmake//android.toolchain.cmake",
    "preferredGenerator": {
      "name": "Ninja"
    },
    "cmakeSettings": {
      "ANDROID_ABI": "arm64-v8a",
      "ANDROID_PLATFORM": "android-33",
      "ANDROID_SDK_ROOT": "C:\\Android",
      "ANDROID_TOOLS_VERSION": "33.0.2",
      "JAVA_HOME": "C:\\jdk-17.0.2"
    }
  }
]
```

Some of the settings above should be changed if you choose to use a different android platform, NDK, ABI or tools version

Save the file, you should now be able to use the compiler by running the command `CMake: Select a Kit` or from the dropdown in the status bar. Select the newly added kit, using the same name you put in the kits configuration file.

--8<-- "includes/license.md"
