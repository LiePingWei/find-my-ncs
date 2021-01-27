# FMN CLI documentation

## Installation

Call the command in the folder containing the `setup.py` file
```
pip install .
```

After you've done this you should be able to call the scripts from your terminal with the command `fmntools`

### Development

Use the editable flag for installation in the development mode:
```
pip install --editable .
```

## Example usage

### Provisioning

Simple provisioning command:

    fmntools provision 
        --device NRF52832 
        --mfi-token l1iyWTwIgR7[...]
        --mfi-uuid abab1212-3434-caca-bebe-cebacebaceba

### Optional arguments

* Set custom output path:

    ```fmntools provision [...] --output-path output.hex```

* Set input hex file to be merged with the provisioning data hex file:

    ```fmntools provision [...] --input-hex-file input.hex```

* Add serial number in a hex format to the provisioned data set:

    ```fmntools provision [...] --serial-number 30313233343536373839414243444546```
