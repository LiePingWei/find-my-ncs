# FMN CLI Tools

FMN CLI Tools is a set of tools that help with the creation of FMN firmware:

 * The [Provision](Provision.md) tool generates a hex file containing the provisioning data
   (MFI UUID and MFI Token).
 * The [Extract](Extract.md) tool extracts the MFi token information from the accessory.
 * The [SuperBinary](SuperBinary.md) tool simplifies composing of the SuperBinary file
   for the FMN Firmware Update.

## Requirements

The tools require Python 3.6 or newer and the pip package installer for Python.
To check the versions, run:

```
pip --version
```

You will see the pip version and the Python version. If you see Python 2, try `pip3` instead
of `pip`.

## Installation

Installation is optional. You can run the Python scripts directly from the sources.

To install the package, run the following command in the folder containing the `setup.py` file:

```
pip install --user .
```

You can skip the `--user` option to install the tools globally for all users in the system.

To install the package in the **development mode**, use the `editable` flag:

```
pip install --editable --user .
```

After this, you can call the scripts from your terminal with the command `ncsfmntools`.

## Uninstallation

To uninstall the package, run:

```
pip uninstall ncsfmntools
```

## Running from the sources

If you skipped the installation, you have to install the Python modules manually:

```
pip install --user intelhex six pynrfjprog
```

Now, you can run the tools with the `python` command. Use the path to the
`ncsfmntools` directory as a first argument:

```
python tools/ncsfmntools
```
