# FMN SuperBinary composing tool

The `fmtools SuperBinary` tool simplifies composing of the SuperBinary file for the
FMN firmware update.

See [FMN CLI Tools](README.md) for details on how to use `ncsfmntools`.

The script can perform the following operations:

 * Updates the SHA-256 of the payloads in the SuperBinary.plist file.
 * Composes the SuperBinary file using the `mfigr2` tool and shows its hash.
 * Creates the release notes ZIP file and shows its hash.
 * Creates the default meta data plist file.

## Requirements

 * Requirements from [FMN CLI Tools](README.md#requirements).
 * The `mfigr2` tool from UARP Developer Kit Tools.
 * macOS operating system with some exceptions.

Add the `mfigr2` tool into the `PATH` environment variable.
Alternatively, you can provide it with the `--mfigr2=file` command-line
argument.

The script works also on a Linux or a Windows machine, but the `mfigr2` tool does
not work on those platforms. To skip executing `mfigr2`, use the `--mfigr2=skip`
command-line argument. This argument also prints information on how to run the
scripts manually on macOS.

## Command line

The `ncsfmntools SuperBinary` tool accepts at most one positional argument:

 * `input`              - Input SuperBinary plist file. The payload hashes from this
                        file will be recalulated. The file will also be used by
			other operations depending on the rest of the arguments.

The following arguments are optional:

 * `--out-plist file`   - Output SuperBinary plist file. Input will be
                        overridden if this argument is omitted.
 * `--metadata file`    - Metadata plist file. If the file does not exist, it
                        will be created with the default values. If you provide the
                        `--out-uarp` argument, the file will be used to compose
                        a SuperBinary file.
 * `--out-uarp file`    - Output composed SuperBinary file. The `mfigr2` tool will
                        be used to compose the final SuperBinary file if you provide
                        this argument.
 * `--payloads-dir path` - The directory containing the payload files for the
                        SuperBinary. By default, it is a directory containing
                        the input SuperBinary plist file.
 * `--release-notes path` - If the path is a directory, the script creates the release notes ZIP
                        file from it and prints its hash. If it is a file,
                        just prints its hash.
 * `--mfigr2 path`      - Custom path to the `mfigr2` tool. By default, `mfigr2`
                        from the PATH environment variable will be used. Setting
                        it to "skip" will only show the commands without
                        executing them.
 * `--debug`            - Show details in case of an exception (for debugging
                        purpose).
 * `--help`             - Show this help message and exit

For example, use the following command to update payload's hashes in the
`SuperBinary.plist` file and compose a SuperBinary from it.

```
ncsfmntools SuperBinary           \
    SuperBinary.plist             \
    --out-uarp SuperBinary.uarp   \
    --metadata Medadata.plist
```

## Samples

The `tools/samples/SuperBinary` directory contains samples of composing
the SuperBinary with the `ncsfmntools SuperBinary`:

 *  The [single](../samples/SuperBinary/single/README.md) sample shows
    how to compose a SuperBinary containing a single payload with one
    application image.
 *  The [incremental](../samples/SuperBinary/incremental/README.md) sample shows
    how to compose a SuperBinary for incremental update with two application images.
 *  The `ReleaseNotes` directory contains sample release notes files that
    the `single` and `incremental` samples use for generating the `ReleaseNotes.zip` file.
