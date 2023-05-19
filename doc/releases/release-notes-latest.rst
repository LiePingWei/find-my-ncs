.. _find_my_release_notes_latest:

.. TODO: Change "latest" in above tag to specific version, e.g. 160

.. TODO: Change "from main branch" to specific version, e.g. v1.6.0

Find My add-on for nRF Connect SDK from main branch
###################################################

.. TODO: Remove following note
.. note::
   This file is a work in progress and might not cover all relevant changes.

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

.. TODO: If there are no highlights, remove the section content below and use the following sentence:
         There are no highlights for this release.

This release covers the following features:

There are no entries for this section yet.

.. TODO: Uncomment following section and change version numbers
  Release tag
  ***********

  The release tag for the Find My add-on for nRF Connect SDK repository is **v0.0.0**.
  This release is compatible with nRF Connect SDK **v0.0.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. TODO: Change main to specific version, e.g. v1.6.0
.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr main

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA20020 (Thingy:52 Prototyping Platform)
* PCA20053 (Thingy:53 Prototyping Platform)

.. TODO: If you adding new kit to this list, add it also to the release-notes-latest.rst.tmpl

Changelog
*********

* Changed error handling behavior in both application and MCUboot bootloader configurations, for all Find My samples as follows:

  * Enabled the :kconfig:option:`CONFIG_RESET_ON_FATAL_ERROR` Kconfig option by default in the Release variant.
  * Disabled the :kconfig:option:`CONFIG_RESET_ON_FATAL_ERROR` Kconfig option by default in the Debug variant.

* Improved the mechanism of injecting the Find My Long Term Key (LTK) into the Bluetooth stack.
  The Bluetooth LE stack no longer associates the Find My connection with any Bluetooth LE bond.
* Added a clearing operation for bond data of the Find My peers during the :c:func:`fmna_enable` API call to avoid pollution of the settings area with unused data.
  You can use the :kconfig:option:`CONFIG_FMNA_BT_BOND_CLEAR` Kconfig option to enable the clearing operation.
  The configuration is disabled by default.
* Added the :kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_FORMAT` Kconfig option to enable printing the MFi Authentication Token in either HEX or Base64 format:

  * The :kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_SHORT` Kconfig option logs only the first 16 bytes of the token in the HEX format.
  * The :kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_HEX_FULL` Kconfig option logs the full token with trimmed trailing zeros in the HEX format.
  * The :kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_SHORT` Kconfig option logs both first and last 16 characters of the Base64 encoded token.
  * The :kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_FULL` Kconfig option logs the full Base64 encoded token.
  * The MFi Authentication Token is now printed in the Base64 format (:kconfig:option:`CONFIG_FMNA_LOG_MFI_AUTH_TOKEN_BASE64_SHORT`) during the Find My initialization.

* Limited log severity to avoid flooding logs while performing UARP transfer.
* Added the :kconfig:option:`CONFIG_FMNA_UARP_LOG_TRANSFER_THROUGHPUT` Kconfig option to enable logging UARP transfer throughput.
* Fixed an issue with applying the application-specific board configuration overlays in the Find My Thingy application.
  The board configuration files were not applied when the application was built from a directory other than the Find My Thingy application directory.
* Fixed an issue in the Softdevice Controller library that used 0 dBm for Find My connection TX power regardless of the :kconfig:option:`CONFIG_FMNA_TX_POWER` Kconfig option value.
  The Find My connection TX power is now correctly inherited from the Find My advertising set as configured by the :kconfig:option:`CONFIG_FMNA_TX_POWER` Kconfig option.
* Fixed an issue with overlaying authentication callbacks using the :c:func:`bt_conn_auth_cb_overlay` function in the :file:`fmna_conn.c` file during the Find My connection establishment.
  This API function is used to enforce the Just Works pairing method and resulted in a NULL pointer dereference, which led to undefined behavior.
  For non-secure targets (nRF5340 DK and Thingy:53), it resulted in a SecureFault exception and a crash.

CLI Tools
=========

* Added the ``-i/--input-file`` option to the ``extract`` command in the Find My CLI tools package, which allows to provide file with settings partition memory dump instead of reading memory directly from the device.
* Reimplemented the algorithm for ``extract`` command in the Find My CLI tools package to closer follow the entry identification logic of the Settings and NVS firmware modules from Zephyr.
* Fixed an issue in the ``extract`` command of the Find My CLI tools package, which caused the older token to be read instead of the latest one.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.

.. TODO:
  1. Before the release, make sure that all TODO items in the 'release-notes-latest.rst' file are fulfilled and deleted.
  2. Change ending of the 'release-notes-latest.rst' file name to an actual version, e.g. 'release-notes-1.6.0.rst'.
  3. After the release, copy the 'release-notes-latest.rst.tmpl' file to the 'release-notes-latest.rst'.
