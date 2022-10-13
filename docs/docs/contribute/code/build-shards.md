---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Build Shards

!!! note
    This guide is for new users unfamiliar with the Shards building process. 
    
    Click [here](#overview) to skip the tutorial and jump to the overview.

Curious about the inner workings of Shards? Build Shards to unlock its hidden magics, and gain access to infinite creative possibilities! ✨ 

Do ensure that you have your [development environment readied](../getting-started.md) before embarking on the quest to build Shards!


## Cloning the Shards Repository ##

Go to the [Shard’s repository on Github](https://github.com/fragcolor-xyz/shards). If you have GitHub for Desktop installed, select the “Code” button, and “Open with GitHub Desktop” to clone the repository. 

![Select “Open with GitHub Desktop” to clone the repository.](assets/github-clone-desktop.png)

??? help ""Open with GitHub Desktop" Error"
    If “Open with GitHub Desktop” does not work, you can instead copy the HTTPS link. 

    ![Copy the HTTPS link of the repository.](assets/github-clone-https-copy.png)

    In GitHub Desktop, select the option to “Clone a Repository” (Ctrl+Shift+O). 
    
    Click on the “URL” tab, paste the HTTPS link and select “Clone”. 

    ![Paste the URL into the empty field and select “Clone”..](assets/github-clone-https-enter.png)

The repository is now cloned to your computer! Take note of where it has been cloned to, as we will be using it in the following step…


## Navigating to the Shards Repository ##

Launch the MinGW terminal (ming64.exe). The .exe file can be found in the msys64 folder, usually located in your C drive.

![Find and select the ming64.exe](assets/location-mingw.png)

Navigate to where your Shards repository is located using the command `cd $(cygpath -u '(X)')`, where (X) is the directory of your folder.

```
cd $(cygpath -u '(X)')
```

??? "cd"
	The `cd` command is used for navigating to different directories within the MinGW terminal.

??? "cygpath"
    The MinGW terminal requires you to convert the backslashes(\\) in your directory address to forward slashes(/). You will also have to remove the colon(:) after the drive alphabet. 
    
    This is the Unix form of an address, and can be done automatically by using the cygpath program.

    e.g. `cygpath -u 'C:\Fragcolor\Shards'` will output `/c/Fragcolor/Shards`

If your Shards repository is located at `C:\Fragcolor\Shards`, the command used in the MinGW terminal would be `cd $(cygpath -u 'C:\Fragcolor\Shards')`.

!!! tip
    A simple way to obtain a folder’s directory is to copy the address as text. 

    Navigate into the folder, right-click the folder in the navigation bar, and select “Copy address as text”.

    ![Copy the directory’s address as text.](assets/location-shards-repo-copy-address.png)

## Updating the Repository and Submodules ##

When new changes are made to the Shards repository, you will want to Pull these changes into your local copy of it. 

To do so with Github Desktop, first select the “Fetch origin” button.

![Select the “Fetch origin” button to check for updates.](assets/github-desktop-fetch.png)

If there are changes to pull, the button will change to “Pull origin”. Select it to update your local repository.

![Select the “Pull origin” button to check for updates.](assets/github-desktop-pull.png)

The Shards repository contains repositories of other projects, known as *submodules*. When updating your Shards repository, these submodules are left alone and must be manually updated. You can update them with the following command:
```
git submodule update --init --recursive
```

??? help "git: command not found"
    This error occurs when WinGW is unable to find where git.exe is. We will have to manually add the path of the folder containing it to the MinGW environment. 

    Use the command `export PATH=$PATH:$(cygpath -u '(X)')` whereby (X) is the folder directory for git.exe. The default location is at `C:\Program Files\Git\cmd`.

    ```
    export PATH=$PATH:$(cygpath -u 'C:\Program Files\Git\cmd')
    ```

??? help "not a git repository"
	This occurs when you are attempting to run the command outside of a git repository. [Navigate to your Shards repository](#navigating-to-the-shards-repository) before attempting to update the submodules.


## Bootstrapping the Project ##

If this is your first time pulling the Shards repository, you will have to run the bootstrap file to set up and build the project.

Enter the command `./bootstrap`.

```
./bootstrap
```

## Creating Build Folders ##

We will next create build folders in the Shards repository - one for the Debug version of Shards, and another for the Release version.

??? "Debug or Release?"
    Debug builds are used when your code is still being tested and you wish to see detailed logs of what happens when your code is run. Release builds are used when you simply want the finished product without excessive logs.

    Debug Build:
    
    ![Debug builds contain detailed logs.](assets/mingw-debug-build.png)

    Release Build:

    ![Release builds hide the detailed logs.](assets/mingw-release-build.png)

First input `mkdir build` in the MinGW terminal to create a folder named “build”. 

```
mkdir build
```

??? "mkdir"
    The `mkdir` command is used to create new folders.


Next, input `cd build` to navigate into the newly created folder.

```
cd build
```

Input `mkdir Debug` to create a folder named “Debug”.

```
mkdir Debug
```

Input `mkdir Release` to create a folder named “Release”.

```
mkdir Release
```

??? "What happened?"
	Look into your Shards repository, you should see the new folders created with the command line!

    ![Build folders created with the command line.](assets/shards-repo-build-folders.png)
    

## Building the Shards Executable ##

We will first build the Debug version of Shards. 

Input the following command to generate the build files:

=== "Command"

    ```
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -B./Debug ..
    ```

=== "Output"

    ```
    -- SHARDS_DIR = C:/Fragcolor/Shards
    -- clang-format found: C:/msys64/mingw64/bin/clang-format.exe
    .
    .
    .
    -- Configuring done
    -- Generating done
    -- Build files have been written to: C:/Fragcolor/Shards/build/Debug
    ```


??? ".."
    `..` represents the parent directory and can be used in the command line to navigate one level up.

    In the command `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -B./Debug ..`, cmake is looking for the file `CMakeLists.txt` which exists one folder above the "build" folder where we are running the command from.


??? help "cmake: command not found"
    This error occurs when MinGW is unable to find where cargo.exe is. We will have to manually add the path of the folder containing it to the MinGW environment. 

    The default location is at `C:\Users\username\.cargo\bin`.

    Use the command `export PATH=$PATH:$(cygpath -u '(X)')` whereby (X) is the folder directory for cargo.exe.

    ```
    export PATH=$PATH:$(cygpath -u '(X)')
    ```

    For the user ‘john’, the command used would be `export PATH=$PATH:$(cygpath -u 'C:\Users\john\.cargo\bin')`.

    You can check if the path has been set correctly by using the command `cargo` in the MinGW terminal. If done correctly, a wall of text starting with “Rust’s package manager” will appear. Otherwise, you will get the error “cargo: command not found”.

??? help "Unable to Build Shards"
	If you are still unable to build Shards, try updating your packages by running:
	```
	pacman -Syu --noconfirm
	```
	```
	rustup update
    ```

Next, input the `ninja` command below to build the .exe file. This might take a few minutes, so feel free to take a coffee break while waiting!

=== "Command"

    ```
    ninja -C Debug shards
    ```

=== "Output"

    ```
    ninja: Entering directory `Debug'
    [0/2] Re-checking globbed directories...
    [1/876] Creating directories for 'sdl_a'
    .
    .
    .
    [874/876] Linking CXX static library lib\libshards-extra.a
    [875/876] Linking CXX static library lib\libshards-core-static.a
    [876/876] Linking CXX executable shards.exe
    ```

The debug version of shards.exe has been built! 🥳

![An executable file for the debug version of shards has been created.](assets/location-shards-debug-exe.png)


We will now repeat the process to create a Release version of Shards. Input the following cmake command:

=== "Command"

    ```
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B./Release ..
    ```

=== "Output"

    ```
    -- The C compiler identification is GNU 12.2.0
    -- The CXX compiler identification is GNU 12.2.0
    .
    .
    .
    -- Configuring done
    -- Generating done
    -- Build files have been written to: C:/Fragcolor/Shards/build/Release
    ```

Once again, build the .exe file with the `ninja` command below. Why not watch a few cat videos while waiting this time?

=== "Command"

    ```
    ninja -C Release shards
    ```

=== "Output"

    ```
    ninja: Entering directory `Release'
    [0/2] Re-checking globbed directories...
    [1/876] Creating directories for 'sdl_a'
    .
    .
    .
    [874/876] Linking CXX static library lib\libshards-extra.a
    [875/876] Linking CXX static library lib\libshards-core-static.a
    [876/876] Linking CXX executable shards.exe
    ```

The release version of shards.exe has been built! 😊

![An executable file for the release version of shards has been created.](assets/location-shards-release-exe.png)


## Overview ##
1. Bootstrap the project if freshly pulled from the repository.
```
./bootstrap
```

2. Create a “build” folder with nested “Debug” and “Release” folders.

3. Use `cmake` commands to generate build files for the Debug and Release versions.
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -B./Debug ..
```
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B./Release ..
```

4. Build the executables with the `ninja` commands.
```
ninja -C Debug shards
```
```
ninja -C Release shards
```

--8<-- "includes/license.md"
