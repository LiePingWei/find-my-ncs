Apple Find My support for nRF Connect SDK
#########################################

The nRF Connect SDK, together with this Find My add-on and nRF52 SoCs, provide a single-chip Bluetooth Low Energy accessory solution that fulfills the requirements defined by the MFi program for the Find My Network (FMN).

nRF Connect SDK
***************

The nRF Connect SDK is hosted, distributed, and supported by Nordic Semiconductor for product development with Nordic SoCs.
The SDK is distributed in multiple Git repositories with application code, drivers, re-distributable libraries, and forks of open-source code used in the SDK.
This add-on is also a part of nRF Connect SDK and requires other SDK repositories to work correctly.
Use the west tool to set up this distributed codebase and to keep it in sync based on the manifest file - :file:`west.yml` from `the main SDK repository <https://github.com/nrfconnect/sdk-nrf>`_.:

For more information about the SDK, see `the nRF Connect SDK documentation  <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html>`_.

Getting started
***************

This section contains basic instructions on how to get started with the development of the Find My accessories. 

Installing the nRF Connect SDK
==============================

Use the west tool to install all components of the nRF Connect SDK, including the Find My add-on. The required version of west is 0.10.0 or higher.

1. Follow the installation instructions in `the nRF Connect SDK Getting started guide <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html>`_.
#. Enable the Find My group filter:

   .. code-block:: none

      west config manifest.group-filter +find-my

#. Update west to download the Find My repository:

   .. code-block:: none

      west update
      // find-my repository is downloaded
      // your github access will be checked

For more information about how west groups work, see `west groups documentation <https://docs.zephyrproject.org/latest/guides/west/manifest.html#project-groups>`_.

Building and running a sample
=============================

.. note::
   Provision your development kit with a HEX file containing the MFi tokens before running any Find My samples.
   See `the ncsfmntools documentation <https://github.com/nrfconnect/sdk-find-my/tree/master/tools/doc>`_ to learn how to generate the HEX file.

To build a Find My sample, complete the following steps:

1. Go to the Find My sample directory:

   .. code-block:: none

      cd <ncs_path>/find-my/samples/<sample_type>

#. Build the sample using west:

   For the Debug configuration, use the following command:

   .. code-block:: none

      west build -b nrf52840dk_nrf52840

   For the Release configuration, use the following command:

   .. code-block:: none

      west build -b nrf52840dk_nrf52840 -- -DCMAKE_BUILD_TYPE=ZRelease

   .. note::
      To use the Release configuration, you need to generate a keypair file as instructed in `the MCUboot documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/mcuboot/readme-zephyr.html#generating-a-new-keypair>`_, name it :file:`mcuboot_private.pem` and place it in ``<ncs_path>/find-my/samples/common/configuration``.

      The Release configuration disables logging in the sample.


#. Connect the development kit to your PC using a USB cable and program the sample or application to it using the following command:

   .. code-block:: none

      west flash

   To fully erase the development kit before programming the new sample or application, use the command:

   .. code-block:: none

      west flash --erase

For more information on building and programming using the command line, see `the Zephyr documentation on Building, Flashing, and Debugging <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/guides/west/build-flash-debug.html#west-build-flash-debug>`_.
