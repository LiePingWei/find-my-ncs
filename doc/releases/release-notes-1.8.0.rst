.. _find_my_release_notes_180:

Find My add-on for nRF Connect SDK v1.8.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Significant optimization of the flash memory footprint in Find My samples.
* Support for nRF Connect extension for Visual Studio Code.
* Improvements to the Find My CLI tools package.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v1.8.0**.
This release is compatible with nRF Connect SDK **v1.8.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v1.8.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)

Changelog
*********

* Ported minimal MCUboot configuration to the release MCUBoot configuration for Find My.
* Reduced MCUboot partition size to 24 kB in the release MCUBoot configuration for Find My.
* Disabled GATT Service Changed and Server Caching features in the Bluetooth configuration for Find My.
* Optimized Mbed TLS configuration for all Find My samples.
* Optimized general release configuration for all Find My samples.
* Added the API Reference to the documentation.
* Added the serial number and UUID extraction to the ``extract`` command in the Find My CLI tools package.
* Improved error handling for the ``extract`` and ``provision`` commands in the Find My CLI tools package.
* Added version information to the Find My CLI tools package.
* Fixed the battery state setting in the nearby and separated advertising packets.
* Fixed the response to the Stop Sound command for non-owner peers.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
