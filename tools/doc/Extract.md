# Extracting tokens

The `Extract` tool extracts the MFi authentication token information from the accessory.

The token is displayed in the Base64 format.

See [FMN CLI Tools](README.md) for details on how to use `ncsfmntools`.

An example of running the tool with the mandatory arguments:

    ncsfmntools extract
        --device NRF52840
        --settings-base 0xfe000
