.. _incremental:

Incremental update SuperBinary
##############################

This sample shows how to compose a SuperBinary for incremental update.

Assume that you have firmware version ``0.0.1`` on your device.
You want to update to version ``0.0.7``, but the device needs a firmware at least in version ``0.0.5`` to be able to perform this update.

The following steps take place in the update procedure:

1. A device running the firmware version ``0.0.1`` looks for the ``FWUP`` payload.

#. The device finds, stages and applies the payload with firmware ``0.0.5``.

#. After a reset, the controller detects version ``0.0.5`` and offers the SuperBinary.

#. The device running the firmware version ``0.0.5`` or above looks for the ``FWU2`` payload.

   To change the expected payload 4CC, use the CONFIG_FMNA_UARP_PAYLOAD_4CC configuration option.

#. The device detects, stages and applies the payload with firmware ``0.0.7``.

#. After a reset, the device is running the desired firmware version.

The build script performs the following steps:

1. Prepare a `build` directory by creating it and copying image files into it from following locations:

   * The :file:`intermediate_update.bin` file from the sample directory is used in the first stage of the update process.
     You have to create this file manually.
   * The :file:`samples/simple/build/zephyr/app_update.bin` file is generated during the application build.
     It is used in the second stage of the update process.

#. Run the `SuperBinary` tool to perform the following operations:

   * Calculate the SHA-256 of the payloads and add it into the :file:`build/SuperBinary.plist` file.
   * Generate the :file:`build/Metadata.plist` file with the default values.
   * Call the mfigr2 tool to compose the :file:`build/SuperBinary.uarp` file.
   * Create the :file:`build/ReleaseNotes.zip` file from the :file:`../ReleaseNotes` directory.
   * Show the SuperBinary hash and the release notes hash needed for releasing the update.

Building
========

To build this sample, complete the following steps:

1. Make sure that the versions in the :file:`SuperBinary.plist` file matches the application versions.
   Also, check the expected 4CC of the payloads.

#. Build your first stage application version and copy the generated :file:`app_update.bin` file to the :file:`intermediate_update.bin` file in the sample directory.

#. Build your second stage application version, for example:

   .. code-block: console

      cd samples/simple
      west build -b nrf52840dk_nrf52840

#. Run the sample build script:

   .. code-block: console

      cd ../../tools/samples/SuperBinary/single
      ./build.sh

