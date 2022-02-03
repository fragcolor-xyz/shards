---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Installing required packages

At Fragcolor we use the Markdown format and MkDocs/ MkDocs-Material for generating the [documentation website](https://docs.fragcolor.xyz/).

In order to contribute changes to Fragcolor's documentation you need to install Git, Python, MkDocs, and MkDocs-Material.

## Git

Git is a version control system that we can use to track and manage changes to our markdown files.

*Git comes pre-installed on most Mac & Linux machines. If it's not on your machine you can get the relevant Git package [here](https://git-scm.com/download).*

For Windows download and install Git from [here](https://git-scm.com/download/win).

You may choose your favorite editor as the default editor for Git. This editor will be invoked in case you need to resolve git merge conflicts etc.

![Choose default Git Editor](assets/install-git_default-editor.png)

Retain the other installation defaults.

## Python

Python is a pre-requisite for both MkDocs and MkDocs Material.

*Python comes pre-installed on most Mac & Linux machines. If it's not on your machine you can get the relevant Python package [here](https://www.python.org/downloads/).*

For windows you can grab the latest Python release installer [here](https://www.python.org/downloads/windows/").

Download the installer and follow the on-screen instructions after starting installation. 

Remember to add Python to Path during the installation.

![Add Python to PATH](assets/install-py_add-to-path.png)

And choose to disable the Path Length Limit (just in case your default installation path is in a deeply nested library).

![Disable path length limit](assets/install-py_disable-pll.png)

Retain the other default installations settings.

To confirm Python was installed successfully type either Python or Py in the windows command prompt. If Python is Installed it will show the version Details.

=== "Command"
    ```
    Python
    ```
=== "Result"
    ![Python was installed successfully](assets/install-py_installed.png)

Typing the following command in the windows command prompt to see where Python is installed.

=== "Command"
    ```
    where py
    ```
=== "Result"
    ![Show where Python is installed](assets/install-py_where-py.png)

Also confirm that PIP (Python's installation package) is installed successfully by typing the following command in the windows command prompt (it's installed automatically with Python):

=== "Command"
    ```
    pip
    ```
=== "Result"
    ![Show where Python is installed](assets/install-py_pip-installed.png) 

*For more details refer to the official Python installation [documentation](https://docs.python.org/3/using/windows.html#installation-steps).*

## MkDocs

MkDocs is a documentation static site generator that makes it easy to create and generate documentation.

Once Python is installed you can install MkDocs using the PIP command from the command prompt.

=== "Command"
    ```
    pip install mkdocs
    ```
=== "Result"
    ![Install MkDocs](assets/install-mk_install.png) 

Then you can check the version of MkDocs installation by using the following command.

=== "Command"
    ```
    mkdocs --version
    ```
=== "Result"
    ![Check MkDocs installation](assets/install-mk_installed.png)

*For more details refer to the official MkDocs installation [documentation](https://www.mkdocs.org/user-guide/installation/#installing-mkdocs).*

## MkDocs Material

MkDocs Material is a theme for MkDocs and can be installed via the PIP command.

=== "Command"
    ```
    pip install mkdocs-material
    ```
=== "Result"
    ![Install MkDocs-Material](assets/install-mkmat_install.png) 

*For more details refer to the official MkDocs getting-started[documentation](https://squidfunk.github.io/mkdocs-material/getting-started/).*



--8<-- "includes/license.md"
