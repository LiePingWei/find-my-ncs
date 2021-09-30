.. _extract:

Extract
#######

The **Extract** tool extracts the following information from the accessory:

* MFi authentication token value
* MFi authentication token UUID
* Serial number

The token is displayed in the Base64 format.
The UUID and the serial number are displayed in the HEX format.

See :ref:`cli_tools` for details on how to use ncsfmntools.

An example of running the tool with the mandatory arguments:

.. code-block:: console

   ncsfmntools extract
       --device NRF52840
       --settings-base 0xfe000

