.. _cli_tools:

CLI Tools
#########

.. contents::
   :local:
   :depth: 2

CLI Tools is a set of tools that help with the creation of FMN firmware.

Toolset
=======

.. toctree::
   :maxdepth: 1
   :glob:

   /tools/doc/extract
   /tools/doc/provision
   /tools/doc/super-binary

SuperBinary samples
===================

.. toctree::
   :maxdepth: 1
   :glob:

   /tools/samples/SuperBinary/single/README
   /tools/samples/SuperBinary/incremental/README
   /tools/samples/SuperBinary/github_action/README

Requirements
============

The tools require Python 3.6 or newer and the pip package installer for Python.
To check the versions, run:

.. code-block:: console

   pip3 --version

Installation
============

.. note::
   Installation is strongly recommended.
   The Find My documentation set assumes that you have the tool installed and accessible from your command window prompt.
   Alternatively, you can run the Python scripts directly from the sources (see the :ref:`cli_tools_running_from_sources` for details).

To install ``ncsfmntools``, open the command prompt window as an administrator in the folder containing this :file:`README.rst` file and the :file:`setup.py` file.
Then, follow the installation section that is relevant for your OS.

macOS installation command
--------------------------

On macOS, enter the following command in a command-line window:

.. code-block:: console

   pip3 install .

.. note::
   Ensure the ``ncsfmntools`` location is added to your system path.

Linux installation command
--------------------------

On Linux, enter the following command in a command-line window:

.. code-block:: console

   pip3 install --user .

.. note::
   Ensure the ``ncsfmntools`` location is added to your system path.

Windows installation command
----------------------------

On Windows, enter the following command in a command-line window:

.. code-block:: console

   pip3 install .

.. note::
   Ensure the ``ncsfmntools`` location is added to the path in environmental variables.

Validation
----------

After the installation, you can use the ``ncsfmntools`` CLI from your command prompt window.

Development mode
----------------

To install the package in the **development mode**, use the ``editable`` flag:

Here is an example command to install ``ncsfmntools`` in the **development mode**  for the Linux environment:

.. code-block:: console

   pip3 install --editable --user .

Uninstallation
==============

To uninstall the package, run:

.. code-block:: console

   pip3 uninstall ncsfmntools

.. _cli_tools_running_from_sources:

Running from the sources
========================

If you skipped the installation, you have to install the dependent Python modules manually before running ``ncsfmntools`` from the sources:

.. code-block:: console

   pip3 install --user intelhex six pynrfjprog

Now, you can run the tools with the ``python`` command.
Use the path to the :file:`ncsfmntools` directory as a first argument:

.. code-block:: console

   python tools/ncsfmntools

