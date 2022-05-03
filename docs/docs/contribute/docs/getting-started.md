---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Getting started

## Documentation requirements

At Fragcolor, we use the following to generate and maintain the [documentation website](https://docs.fragcolor.xyz/).

- MkDocs & MkDocs Material - Documentation static site generator
- Python - Prerequisite for MkDocs & MkDocs Material
- git & GitHub - Document version control & contribution workflow
- VS Code - Code & document editor

We follow the [Microsoft Style Guide](https://docs.microsoft.com/en-us/style-guide/welcome/) to standardize our documentation.

## Documentation build environment

### Git & GitHub

Git is a version control system that we can use to track and manage changes to our markdown files.

*Git comes pre-installed on most Mac & Linux machines. If it's not on your machine, you can get the relevant git package [here](https://git-scm.com/download).*

For Windows, download and install git from [here](https://git-scm.com/download/win).

Choose your favorite editor as the default git editor. Git will use this editor when there's a need to resolve merge conflicts.

![Choose default git Editor](assets/install-git_default-editor.png)

Keep the other installation defaults.

You'll can download/ clone the documentation repository via the HTTPS protocol. But you'll need an active [GitHub](https://github.com/) account to fork the documentation repository or contribute to it.

### Python

Python is a pre-requisite for both MkDocs and MkDocs Material.

*Python comes pre-installed on most Mac & Linux machines. If it's not on your machine you can get the relevant Python package [here](https://www.python.org/downloads/).*

For windows, you can grab the latest Python release installer [here](https://www.python.org/downloads/windows/).

Download the installer and follow the on-screen instructions after starting the installation. 

Remember to add Python to Path during the installation.

![Add Python to PATH](assets/install-py_add-to-path.png)

Disable the Path Length Limit so that your installation can use nested paths.

![Disable path length limit](assets/install-py_disable-pll.png)

Keep the other default installation settings.

Type `Python` or `Py` in the terminal to confirm Python installation and version.

=== "Command"

    ```
    Python
    ```

=== "Result"

    ![Python was installed successfully](assets/install-py_installed.png)

Type `where py` in the terminal to see the Python installation path.

=== "Command"

    ```
    where py
    ```

=== "Result"

    ![Show where Python is installed](assets/install-py_where-py.png)

Type `pip` to confirm PIP installation and version.
*PIP is Python Installation Package and installs as part of Python.*

=== "Command"

    ```
    pip
    ```

=== "Result"

    ![Show where Python is installed](assets/install-py_pip-installed.png)

*For more details, refer to the official Python installation [documentation](https://docs.python.org/3/using/windows.html#installation-steps).*

### MkDocs

MkDocs allows you to write documentation in markdown and then serve it as a static site. 

Install MkDocs using the `pip` tool.

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

*For more details, refer to the official MkDocs installation [documentation](https://www.mkdocs.org/user-guide/installation/#installing-mkdocs).*

### MkDocs Material

MkDocs Material is a Material Design theme for MkDocs.

Install MkDocs Material using the `'pip` tool.

=== "Command"

    ```
    pip install mkdocs-material
    ```

=== "Result"

    ![Install MkDocs-Material](assets/install-mkmat_install.png)

*For more details, refer to the official MkDocs getting-started [documentation](https://squidfunk.github.io/mkdocs-material/getting-started/).*


--8<-- "includes/license.md"
