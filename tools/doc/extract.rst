.. _extract:

Extract
#######

The **Extract** tool extracts the MFi authentication token information from the accessory.

The token is displayed in the Base64 format.

See :ref:`cli_tools` for details on how to use ncsfmntools.

An example of running the tool with the mandatory arguments:

.. code-block:: console

   ncsfmntools extract
       --device NRF52840
       --settings-base 0xfe000

