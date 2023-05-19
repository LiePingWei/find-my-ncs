.. _find_my_release_notes_210:

Find My add-on for nRF Connect SDK v2.1.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Added support for nRF5340 SoC in the Find My stack and samples.
* Added a dedicated API for turning off the Find My functionality.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.1.0**.
This release is compatible with nRF Connect SDK **v2.1.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.1.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA20020 (Thingy:52 Prototyping Platform)

Changelog
*********

* Added a new :c:func:`fmna_disable` function to the Find My API. It disables the FMN stack.
* Added a new :c:func:`fmna_is_ready` function to the Find My API. It checks if the FMN stack is enabled.
* Added a mechanism for activating and deactivating Find My on the long **Button 1** press with the status indication displayed on the **LED 4**.

  This change is available in the Find My Simple and Qualification samples.
* Added a new callback to the Find My API. It notifies the user about the paired state changes.
* Added an indication of the paired state using **LED 3** in the Find My Simple and Qualification samples.
* Pairing mode timeout is no longer restarted on the accessory once a Find My pairing candidate connects to it.
* Added a new :kconfig:option:`CONFIG_FMNA_PAIRING_MODE_TIMEOUT` configuration option for setting the pairing mode timeout.
* Added a new :kconfig:option:`CONFIG_FMNA_PAIRING_MODE_AUTOSTART` configuration option for disabling automatic advertising in the pairing mode.

  When this option is disabled and the accessory is unpaired, you need to start advertising using the :c:func:`fmna_resume` function.
* Blinking **LED 3** indicates active pairing mode in the Find My Simple and Qualification samples.
* Enabling the pairing mode by pressing **Button 1** is now required in the Find My Simple and Qualification samples.
* Added a configuration file for the HCI RPMsg child image in all Find My samples.

  This file is used with the nRF5340 targets and aligns the application image with its network counterpart.
* Added support for the Release configuration on the nRF5340 target.
* Extended the settings storage partition for the nRF5340 target to fill up the space left because of its 4-page alignment.
* Added support for the non-secure nRF5340 target.
* Aligned the Find My Serial Number module with TF-M and SPM requirements for non-secure targets.
* Migrated to the full Oberon implementation of crypto primitives for Find My.
* The Device ID from the FICR register group is now used as a serial number for Find My.
* Replaced static verification of the TX power parameter ``FMNA_TX_POWER`` with a dynamic one.

  A warning is logged in case of a mismatch between the chosen TX power and platform capabilities during Find My initialization.
* Added support for a common target-based partition configuration in Find My samples.
* Added support for a common target-based Kconfig configuration for the primary application and MCUboot image.

CLI Tools
=========

* Added the nRF5340 SoC support to the provision and extract command in the v0.2.0 release.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3dBm, violating the Find My specification requirement for 4dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Find My pairing may sometimes fail due to the connection timeout (especially in the Find My Coexistence sample).
  The root cause of this behaviour is a low value of the link supervision timeout parameter.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
* The Find My Thingy application does not support the Thingy:53 platform.
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
