.. _samples:

Samples
#######

.. contents::
   :local:
   :depth: 2

The Find My add-on provides a set of samples that demonstrate how to work with the Find My stack.

Find My samples
===============

   .. toctree::
      :maxdepth: 1
      :glob:

      ../samples/*/README

Build configurations
====================

You can build each Find My sample in the following configurations:

- The Release configuration called *ZRelease*
- The Debug configuration called *ZDebug*

To select the build configuration, populate the CMAKE_BUILD_TYPE variable in the build process.
See the example below for reference:

.. code-block:: console

   west build -b nrf52dk_nrf52832 -- -DCMAKE_BUILD_TYPE=ZRelease

The *ZDebug* build configuration is used by default if the CMAKE_BUILD_TYPE variable is not specified

Your build command displays the information about the selected build configuration at the beginning of its log output.
For the Release configuration, it would look like this:

.. code-block:: console

   -- Build type: ZRelease

.. note::
   To use the Release configuration, you need to generate a keypair file as instructed in `the MCUboot documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/mcuboot/readme-zephyr.html#generating-a-new-keypair>`_, name it :file:`mcuboot_private.pem` and place it in :file:`<ncs_path>/find-my/samples/common/configuration`.

   The Release configuration disables logging in the sample.

.. _samples_building:

Building and running a sample
=============================

.. note::
   Configure your MFi product plan in a Find My sample before building and running it.
   Then, provision your development kit with a HEX file containing the MFi tokens that belong to your plan.
   See :ref:`find_my_pairing` for more details.

To build a Find My sample, complete the following steps:

1. Go to the Find My sample directory:

   .. code-block:: console

      cd <ncs_path>/find-my/samples/<sample_type>

#. Build the sample using west:

   For the Debug configuration, use the following command:

   .. code-block:: console

      west build -b nrf52840dk_nrf52840

   For the Release configuration, use the following command:

   .. code-block:: console

      west build -b nrf52840dk_nrf52840 -- -DCMAKE_BUILD_TYPE=ZRelease

#. Connect the development kit to your PC using a USB cable and program the sample or application to it using the following command:

   .. code-block:: console

      west flash

   To fully erase the development kit before programming the new sample or application, use the command:

   .. code-block:: console

      west flash --erase

For more information on building and programming using the command line, see `the Zephyr documentation on Building, Flashing, and Debugging <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/guides/west/build-flash-debug.html#west-build-flash-debug>`_.
