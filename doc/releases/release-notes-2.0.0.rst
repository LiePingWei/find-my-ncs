.. _find_my_release_notes_200:

Find My add-on for nRF Connect SDK v2.0.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* The webpage with information about the found item is now correctly displayed
  during the Identify Found Item UI flow in the Find My iOS application.
* Removed the Bluetooth limitation that prevented the *pair before use* accessory
  from maintaining multiple connections with the same peer.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.0.0**.
This release is compatible with nRF Connect SDK **v2.0.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.0.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA20020 (Thingy:52 Prototyping Platform)

Changelog
*********

* Fixed the serial number counter to start from zero instead of one.
* Fixed an array overwrite when the serial number string is 16 bytes long.
* Populated unset memory regions of variables that are used during serial number encryption.
* Serial number counter is now correctly incremented after each successful NFC read operation.
* Find My NFC payload is now updated along with the serial number counter.
* Removed a limitation from the Softdevice Controller library.

  It prevented the Bluetooth stack from maintaining multiple connections with the same peer (address-based filtration).
* In the Find My Coexistence sample, fixed an issue that prevented maintaining two simultaneous connections
  with the same iOS device: one for the Heart Rate sensor use case and the other for the Find My use case.
* Fixed out-of-bounds access for crypto key derivation operation.

  The access was triggered during the error handling exit from the function.
* Removed the ``fmna_conn_check`` API function that was unreliable with the disabled Find My stack.
* Increased the stack size of the Bluetooth RX thread to avoid stack overflow during Find My pairing.

  The configuration is changed in all Find My samples and applications for both the release and the debug variant.
* Enabled custom project configuration for MCUboot child image.

  The configuration works as an extension to the common configuration of all Find My samples and applications.

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
