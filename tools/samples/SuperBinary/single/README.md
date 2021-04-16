# Single payload SuperBinary sample

This sample shows how to compose a SuperBinary containing a single payload with one application image.

The `SuperBinary.plist` file defines a SuperBinary version `0.0.1` with payload also version `0.0.1`.

The build script performs the following steps:

1.  Create a `build` directory and copy the
    `apps/simple/build/zephyr/app_update.bin` file to it. This file
    is generated during the application build. If you are not using
    the default build directory for application building, you have to
    change this path in the script.
2.  Run the `ncsfmntools SuperBinary` tool to perform the following operations:
    *  Calculate the SHA-256 of the payload and put it into the `build/SuperBinary.plist` file.
    *  Generate the `build/Metadata.plist` file with the default values.
    *  Call the `mfigr2` tool to compose the `build/SuperBinary.uarp` file.
    *  Create the `build/ReleaseNotes.zip` from the `../ReleaseNotes` directory.
    *  Show the SuperBinary hash and the release notes hash needed for releasing the update.

## Building

To build this sample, complete the following steps:

1.  Make sure that the version in the `SuperBinary.plist` file matches the
    application version.

2.  Build the application with `west`:
    ```
    cd apps/simple
    west build -b nrf52840dk_nrf52840
    ```

3.  Run the sample build script:
    ```
    cd ../../fmn/tools/samples/SuperBinary/single
    ./build.sh
    ```