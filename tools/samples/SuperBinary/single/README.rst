.. _super_single:

Single payload SuperBinary
##########################

This sample shows how to compose a SuperBinary containing a single payload with one application image.

The :file:`SuperBinary.plist` file defines a SuperBinary version `0.0.1` with payload also version `0.0.1`.

The build script performs the following steps:

1. Create a `build` directory and copy the :file:`samples/simple/build/zephyr/app_update.bin` file to it.
   This file is generated during the application build.
   If you are not using the default build directory for application building, you have to change this path in the script.

#. Run the SuperBinary tool to perform the following operations:

   * Calculate the SHA-256 of the payload and put it into the :file:`build/SuperBinary.plist` file.
   * Generate the :file:`build/Metadata.plist` file with the default values.
   * Call the mfigr2 tool to compose the :file:`build/SuperBinary.uarp` file.
   * Create the :file:`build/ReleaseNotes.zip` file from the :file:`../ReleaseNotes` directory.
   * Show the SuperBinary hash and the release notes hash needed for releasing the update.

Building
========

To build this sample, complete the following steps:

1. Make sure that the version in the `SuperBinary.plist` file matches the application version.

#. Build the application with ``west``:

   .. code-block: console

      cd samples/simple
      west build -b nrf52840dk_nrf52840

#. Run the sample build script:

   .. code-block: console
    
      cd ../../tools/samples/SuperBinary/single
      ./build.sh

