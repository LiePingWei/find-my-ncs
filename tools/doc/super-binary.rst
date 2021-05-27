.. _super-binary:

SuperBinary
###########

The **SuperBinary** tool simplifies composing of the SuperBinary file for the FMN firmware update.

See :ref:`cli_tools` for details on how to use ncsfmntools.

The script can perform the following operations:

* Update the SHA-256 of the payloads in the :file:`SuperBinary.plist` file.
* Compose the SuperBinary file using the mfigr2 tool and show its hash.
* Create the release notes ZIP file and show its hash.
* Create the default meta data plist file.

Requirements
============

* Requirements from :ref:`cli_tools`.
* The mfigr2 tool from the UARP Developer Kit Tools.
* macOS operating system with some exceptions.

Add the mfigr2 tool into the PATH environment variable.
Alternatively, you can provide it with the ``--mfigr2=file`` command-line argument.

The script works also on a Linux or a Windows machine, but the mfigr2 tool does not work on those platforms.
To skip executing mfigr2, use the ``--mfigr2=skip`` command-line argument.
This argument also prints information on how to run the scripts manually on macOS.

Command line
============

The SuperBinary tool accepts at most one positional argument:

* ``input`` - Input SuperBinary plist file. The payload hashes from this file are be recalulated.
  The file is also be used by other operations depending on the rest of the arguments.

The following arguments are optional:

* ``--out-plist file`` - Output SuperBinary plist file.
  Input is overridden if this argument is omitted.

* ``--metadata file`` - Metadata plist file.
  If the file does not exist, it is created with the default values.
  If you provide the ``--out-uarp`` argument, the file is used to compose a SuperBinary file.

  ``--out-uarp file`` - Output composed SuperBinary file.
  The mfigr2 tool is used to compose the final SuperBinary file if you provide this argument.

* ``--payloads-dir path`` - The directory containing the payload files for the SuperBinary.
  By default, it is a directory containing the input SuperBinary plist file.

* ``--release-notes path`` - If the path is a directory, the script creates the release notes ZIP file from it and prints its hash.
  If the path has a file, this argument just prints its hash.

* ``--mfigr2 path`` - Custom path to the mfigr2 tool.
  By default, mfigr2 from the PATH environment variable is used.
  Setting it to "skip" only shows the commands without executing them.

* ``--debug`` - Show details in case of an exception (for debugging purpose).

* ``--help`` - Show this help message and exit.

For example, use the following command to update the payload's hashes in the :file:`SuperBinary.plist` file and compose a SuperBinary from it.

.. code-block:: console

   ncsfmntools SuperBinary           \
      SuperBinary.plist             \
      --out-uarp SuperBinary.uarp   \
      --metadata Medadata.plist

Samples
=======

The :file:`tools/samples/SuperBinary` directory contains samples of composing the SuperBinary with the SuperBinary tool:

* The :ref:`super_single` sample shows how to compose a SuperBinary containing a single payload with one application image.
* The :ref:`incremental` sample shows how to compose a SuperBinary for incremental update with two application images.
* The :file:`ReleaseNotes` directory contains sample release notes files that the single and incremental samples use for generating the :file:`ReleaseNotes.zip` file.

