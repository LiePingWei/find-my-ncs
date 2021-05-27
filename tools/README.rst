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

Requirements
============

The tools require Python 3.6 or newer and the pip package installer for Python.
To check the versions, run:

.. code-block:: console

   pip --version

You will see the pip version and the Python version. If you see Python 2, try ``pip3`` instead of ``pip``.

Installation
============

Installation is optional.
You can run the Python scripts directly from the sources.

To install the package, run the following command in the folder containing the :file:`setup.py` file:

.. code-block:: console

   pip install --user .

You can skip the ``--user`` option to install the tools globally for all users in the system.

To install the package in the **development mode**, use the ``editable`` flag:

.. code-block:: console

   pip install --editable --user .

After this, you can call the scripts from your terminal with the command ``ncsfmntools``.

Uninstallation
==============

To uninstall the package, run:

.. code-block:: console

   pip uninstall ncsfmntools

Running from the sources
========================

If you skipped the installation, you have to install the Python modules manually:

.. code-block:: console

   pip install --user intelhex six pynrfjprog

Now, you can run the tools with the ``python`` command.
Use the path to the :file:`ncsfmntools` directory as a first argument:

.. code-block:: console

   python tools/ncsfmntools

