.. _find_my_pairing:

Find My pairing preparations
############################

The following subsections describe how to prepare the Find My compatible accessory firmware to pair it successfully with the Find My iOS application.

Product plan
************

To pair with the Find My iOS application, you have to configure the Find My accessory firmware to use a valid MFi product plan.
Each MFi partner using this SDK, should set their product plan configuration accordingly.

By default, all Find My samples in this SDK use Nordic Semiconductor product plan.
This setting is indicated by the CONFIG_FMNA_NORDIC_PRODUCT_PLAN configuration.
You need to have a dedicated product plan for your accessory application development.

Before you start to configure your product plan, make sure that the CONFIG_FMNA_NORDIC_PRODUCT_PLAN configuration is disabled.
The following sections demonstrate how to set specific parameters for your plan.

Product Data (PD)
=================

To configure the Product Data, define a global variable with a specific name.
The Find My stack will access it on demand in the runtime.

For example, Product Data *f86ae43ad21a6d93* corresponds to the following global variable definition:

.. code-block:: c

   /* Product Data configuration: PD */
   const uint8_t fmna_pp_product_data[] = {
           0xf8, 0x6a, 0xe4, 0x3a, 0xd2, 0x1a, 0x6d, 0x93
   };

Define this configuration in the :file:`main.c` file or any other application source file.

Apple server keys
=================

Each product plan contains the configuration for Apple server keys.
Two different keys are relevant for the Find My project configuration:

* Server encryption key
* Signature verification key

The encryption key and signature verification key found in the product plan are Base64 encoded.
You must decode both of them to byte arrays to fit the format used in the source code.

Similarly to the Product Data, configure the server keys by defining global variables with appropriate names:

.. code-block:: c

   /* Server encryption key: Q_E */
   const uint8_t fmna_pp_server_encryption_key[] = {
           (...)
   };
   
   /* Server signature verification key: Q_A */
   const uint8_t fmna_pp_server_sig_verification_key[] = {
           (...)
   };

For example, the Server Encryption Key (Base64 encoded)
*BJzFrd3QKbdTXTDm5dFtt6jSGxtItVsZ1bEQ6VvzFUXndM9Rjeu+PHFoM+RD8RRHblpLBU42dQcFbjmVzGuWkJY=*
corresponds to the following byte array definition:

.. code-block:: c

   const uint8_t fmna_pp_server_encryption_key[] = {
           0x04, 0x9c, 0xc5, 0xad, 0xdd, 0xd0, 0x29, 0xb7, 0x53, 0x5d, 0x30, 0xe6, 0xe5,
           0xd1, 0x6d, 0xb7, 0xa8, 0xd2, 0x1b, 0x1b, 0x48, 0xb5, 0x5b, 0x19, 0xd5, 0xb1,
           0x10, 0xe9, 0x5b, 0xf3, 0x15, 0x45, 0xe7, 0x74, 0xcf, 0x51, 0x8d, 0xeb, 0xbe,
           0x3c, 0x71, 0x68, 0x33, 0xe4, 0x43, 0xf1, 0x14, 0x47, 0x6e, 0x5a, 0x4b, 0x05,
           0x4e, 0x36, 0x75, 0x07, 0x05, 0x6e, 0x39, 0x95, 0xcc, 0x6b, 0x96, 0x90, 0x96
   };

Accessory Information Service configuration
===========================================

You also need to configure accordingly other product plan parameters, that are used in the Accessory Information Service.
Examples of such parameters are:

* Manufacturer Name
* Model Name
* Accessory Category
* Accessory Capabilities
* Battery Type

To set these parameters for your project, use the Find My Kconfig configuration.

Working with MFi tokens
***********************

To pair with the Find My iOS application, you have to provision your accessory with the following MFi tokens:

* Software Authentication UUID
* Software Authentication Token

The MFi tokens are generated for the specific product plan.
You can only use them with the plan for which they were generated.

Provisioned data generation
===========================

The *ncsfmntools* Python package provides support for provisioning Find My accessories.
The package contains a command-line interface (CLI) that you can use for MFi token provisioning.

Refer to the :ref:`provision` command documentation for examples on how to generate a HEX file with provisioned data.

Memory location of provisioned data
===================================

The provisioned data is stored by the Settings subsystem with Non-Volatile Storage (NVS) module as its backend.
NVS is a file system library available in the nRF Connect SDK.
The file system uses a fragment of the device memory for storing user's data.

For more information about storage system, see 
`the Settings documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/services/settings/index.html>`_ and
`the Non-Volatile Storage documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/services/storage/nvs/nvs.html>`_.

In the default configuration of the Find My application, the NVS memory region occupies last two pages of the device memory.
By default, the *ncsfmntools* package generates a HEX file with provisioned data that is compatible with this firmware configuration.

Provisioned data programming
============================

To program the HEX file with provisioned data, generated by the *ncsfmntools* package, onto the device, use the *nrfjprog* CLI.
The *nrfjprog* CLI is part of the nRF Command Line Tools software.

Program the whole application with provisioned data as follows:

1. Generate the HEX file with provisioned data using *ncsfmntools* CLI.
2. Erase the device memory with ``nrfjprog -e``.
3. Program the provisioned HEX file with ``nrfjprog --program provisioned.hex``.
4. Program the application project using the SEGGER Embedded Studio GUI or use ``west`` CLI as outlined in the :ref:`samples_building` section.

Instead of the SEGGER, you can also program the application HEX file, SoftDevice and reset the device using the *nrfjprog* CLI.

It is important to populate the NVS memory region with the provisioned data before the application starts and initializes this region with the default values.
This programming procedure guarantees that this requirement is fulfilled.
Otherwise, it will only be possible to program the HEX file with provisioned data with the ``--sectorerase`` *nrfjprog* option.

Token validation
================

Use the logs to validate that you performed the provisioning operation of MFi tokens correctly.
Make sure that logging is enabled in your application and the logging level for the Find My libraries is set to *INFO* or *DEBUG*.

After a successful provisioning, you should see the logs like in the example output below:

.. code-block:: console

   I: SW UUID:
   I: 87 40 6b 69 e8 27 4a a3 |.@ki.'J.
   I: 9d d2 b3 3d 45 51 88 3e |...=EQ.>
   I: SW Authentication Token:
   I: 31 81 be 30 4e 02 01 01 |1..0N...
   I: 02 01 01 04 46 30 44 02 |....F0D.
   I: (... 1008 more bytes ...)

In case of a failure, you will see the following information:

.. code-block:: console

   W: MFi Token UUID not found: please provsion a token to the device
   W: MFi Authentication Token not found: please provsion a token to the device

Loss of Authentication Token
============================

The Software Authentication Token is updated in the device memory at each successful pairing with the Find My iOS application.
You cannot use the initial value of the Authentication Token to pair with the iOS application in subsequent pairing attempts.
Store the updated token information before erasing the device.
In this case, use the :ref:`extract` command from the *ncsfmntools* package.
If the token is erased without any backup, there is no way of recovering it.
In such case, you need to use a new MFi token to pair with the device.
