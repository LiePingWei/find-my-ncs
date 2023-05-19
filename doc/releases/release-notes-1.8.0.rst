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
* Find My pairing may sometimes fail due to the connection timeout (especially in the Find My Coexistence sample).
  The root cause of this behaviour is a low value of the link supervision timeout parameter.
* In certain corner cases, the settings storage gets permanently polluted with invalid GATT data that are associated with Find My connections.
  The pollution may be caused by the following settings items handled by the GATT layer:

    * Client Characteristic Configuration (CCC) descriptor: regardless of application configuration options.
    * Client Features (CF) status: if you enabled the :kconfig:option:`CONFIG_BT_GATT_CACHING` configuration option.
    * Service Changed (SC) status: if you enabled the :kconfig:option:`CONFIG_BT_GATT_SERVICE_CHANGED` configuration option.
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.4.0** tag.

  **Workaround** (for in-field products affected by this issue):

  Migrate your firmware to the **v2.4.0** nRF Connect SDK release or newer and enable the :kconfig:option:`CONFIG_FMNA_BT_BOND_CLEAR` Kconfig option to automatically clear the settings storage pollution during the :c:func:`fmna_enable` function.
  Depending on your DFU capabilities and preference, you can choose one of the following approaches of delivering the fix to your customers:
    * If your DFU method supports the incremental updates feature, you can specify a requirement that an accessory must be running a specific firmware version to update to the newer version.
      In this case, you can prepare two DFU packages with the following properties:

        1. The older version with the :kconfig:option:`CONFIG_FMNA_BT_BOND_CLEAR` option enabled.
        #. The newer one with the :kconfig:option:`CONFIG_FMNA_BT_BOND_CLEAR` option disabled and a requirement to trigger an update only for the firmware version from the first package.
    * If your DFU method does not support incremental updates, prepare one DFU package with the :kconfig:option:`CONFIG_FMNA_BT_BOND_CLEAR` option enabled.
      You can disable this option in one of the future updates (for example, within a year) once you are confident that your users have their settings storage cleared with the initial update.
