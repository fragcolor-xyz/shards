---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Start Documenting

!!! note
    This guide is for new users unfamiliar with the installation process. 

    Click [here](#overview) to skip the tutorial and jump to the overview.

Eager to help contribute to our ever-growing library of documentation? Here is how to get started!

Our documentation follows the [Microsoft Style Guide](https://docs.microsoft.com/en-us/style-guide/welcome/). Do refer to it if you are unsure of the documentation style!

In order to start writing documentation for Fragcolor, ensure that you have [readied your development environment](../getting-started.md) first. Once done, we can proceed with the setting up of our documentation site generator.

## Python

Python needs to be installed for the site generator to work. Download Python [here](https://www.python.org/downloads/) and follow the instructions given by the installer. Remember to add Python to PATH during the installation!

![Add Python to PATH](assets/install-py-add-to-path.png)

By adding Python to PATH, it allows your terminals to be able to find and use Python. Try typing `Python` in any terminal to check if the installation process is successful.

=== "Command"

    ```
    Python
    ```

=== "Result"

    ![Python was installed successfully](assets/install-py-results.png)


## MkDocs

MkDocs is a static site generator that allows you to write documentation in markdown. 

??? help "What is Markdown?"
    It is a formatting language used to stylize plain text. Take a look at how this segment is written in markdown!

    ![How markdown works.](assets/what-is-markdown.png)

We will install MkDocs using the Package Installer for Python (pip). Run the following command in your terminal:

=== "Command"

    ```
    pip install mkdocs
    ```

Once done, you can check if your installation was successful by running the following command:

=== "Command"

    ```
    mkdocs
    ```

=== "Result"

    ![Check MkDocs installation.](assets/mkdocs-installation-results.png)


We will next install a theme for MkDocs with the command:

=== "Command"

    ```
    pip install mkdocs-material
    ```

You are now ready to start writing documentation! We will learn how Fragcolor's documentation is written in the next chapter.

## Overview ##

1. Download [Python](https://www.python.org/downloads/) and add it to PATH.

2. Install MkDocs.
```
pip install mkdocs
```

3. Install Material for MkDocs.
```
pip install mkdocs-material
```

--8<-- "includes/license.md"
