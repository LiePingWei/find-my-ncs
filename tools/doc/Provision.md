# Provisioning

The `Provision` tool generates a hex file containing the provisioning data.

See [FMN CLI Tools](README.md) for details on how to use `ncsfmntools`.

An example of running the tool with the mandatory arguments:

    ncsfmntools provision
        --device NRF52840
        --mfi-token l1iyWTwIgR7[...]
        --mfi-uuid abab1212-3434-caca-bebe-cebacebaceba
        --settings-base 0xfe000

## Optional arguments

You can also add these arguments to further specify the resulting hex file:

* Set custom output path:

    `ncsfmntools provision [...] --output-path output.hex`

* Set the input hex file to be merged with the provisioning data hex file:

    `ncsfmntools provision [...] --input-hex-file input.hex`

* Add a serial number in hex format to the provisioned data set:

    `ncsfmntools provision [...] --serial-number 30313233343536373839414243444546`

* Set a custom settings base address:

    `ncsfmntools provision [...] --settings-base 0xfe000`
