.. _provision:

Provision
#########

The **Provision** tool generates a HEX file containing the provisioning data.

See :ref:`cli_tools` for details on how to use ncsfmntools.

An example of running the tool with the mandatory arguments:

.. code-block:: console

   ncsfmntools provision
       --device NRF52840
       --mfi-token l1iyWTwIgR7[...]
       --mfi-uuid abab1212-3434-caca-bebe-cebacebaceba
       --settings-base 0xfe000

Optional arguments
==================

You can also add these arguments to further specify the resulting HEX file:

* Set custom output path:

  .. code-block:: console

     ncsfmntools provision [...] --output-path output.hex

* Set the input HEX file to be merged with the provisioning data HEX file:

  .. code-block:: console

     ncsfmntools provision [...] --input-hex-file input.hex

* Add a serial number in HEX format to the provisioned data set:

  .. code-block:: console

     ncsfmntools provision [...] --serial-number 30313233343536373839414243444546

* Set a custom settings base address:

  .. code-block:: console

     ncsfmntools provision [...] --settings-base 0xfe000

