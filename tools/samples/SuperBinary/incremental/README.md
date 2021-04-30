# Incremental update SuperBinary sample

This sample shows how to compose a SuperBinary for incremental update.

Assume that you have firmware version `0.0.1` on your device. You want
to update to version `0.0.7`, but the device needs a firmware at least in
version `0.0.5` to be able to perform this update.

The following steps take place in the update procedure:

1.  A device running the firmware version `0.0.1` looks for the `FWUP` payload.

2.  The device finds, stages and applies the payload with firmware `0.0.5`.

3.  After a reset, the controller detects version `0.0.5` and offers the SuperBinary.

4.  The device running the firmware version `0.0.5` or above looks for the `FWU2` payload.

    To change the expected payload 4CC, use the `CONFIG_FMNA_UARP_PAYLOAD_4CC` 
    configuration option.

5.  The device detects, stages and applies the payload with firmware `0.0.7`.

6.  After a reset, the device is running the desired firmware version.

The build script performs the following steps:

1.  Prepare a `build` directory by creating it and copying
    image files into it from following locations:
    *  `intermediate_update.bin` from the sample directory is used in the first
       stage of the update process. You have to create this file manually.
    *  `samples/simple/build/zephyr/app_update.bin` is generated during the application
       build. It is used in the second stage of the update process.
2.  Run the `ncsfmntools SuperBinary` tool to perform the following operations:
    *  Calculate the SHA-256 of the payloads and put it into the `build/SuperBinary.plist` file.
    *  Generate the `build/Metadata.plist` file with the default values.
    *  Call the `mfigr2` tool to compose the `build/SuperBinary.uarp` file.
    *  Create the `build/ReleaseNotes.zip` from the `../ReleaseNotes` directory.
    *  Show the SuperBinary hash and the release notes hash needed for releasing the update.

## Building

To build this sample, complete the following steps:

1.  Make sure that the versions in the `SuperBinary.plist` file matches the
    application versions. Also, check the expected 4CC of the payloads.

2.  Build your first stage application version and copy the generated
    `app_update.bin` file to the `intermediate_update.bin` file in the sample directory.

3.  Build your second stage application version, for example:
    ```
    cd samples/simple
    west build -b nrf52840dk_nrf52840
    ```

4.  Run the sample build script:
    ```
    cd ../../fmn/tools/samples/SuperBinary/single
    ./build.sh
    ```
