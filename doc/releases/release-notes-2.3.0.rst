.. _find_my_release_notes_230:

Find My add-on for nRF Connect SDK v2.3.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

There are no highlights for this release.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.3.0**.
This release is compatible with nRF Connect SDK **v2.3.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.3.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA10095 (nRF5340 Development Kit)
* PCA20020 (Thingy:52 Prototyping Platform)
* PCA20053 (Thingy:53 Prototyping Platform)

Changelog
*********

* Disabled the external flash support for MCUboot in all Find My samples and applications.
* Disabled the network core upgrade support on nRF53 platforms in all Find My samples and applications.
* Fixed an issue with the board configurations of the Find My Thingy application.
  The board configuration files were not applied for this application.
* Fixed the configuration of the Find My Thingy application for the Thingy:53 non-secure target.
* The :kconfig:option:`CONFIG_HEAP_MEM_POOL_SIZE` configuration option must be set explicitly in Find My projects.
  The default value for the Find My use case is no longer applied automatically.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
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
* The Softdevice Controller library incorrectly uses 0 dBm for Find My connection TX power regardless of the :kconfig:option:`CONFIG_FMNA_TX_POWER` Kconfig option value.
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.4.0** tag.
* Overlaying authentication callbacks using the :c:func:`bt_conn_auth_cb_overlay` function in the :file:`fmna_conn.c` file during the Find My connection establishment results in a NULL pointer dereference, which leads to undefined behavior.
  This API function is used to enforce the Just Works pairing method.
  For non-secure targets (nRF5340 DK and Thingy:53), it results in a SecureFault exception and a crash.
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.4.0** tag.
  **Workaround:** Manually cherry-pick and apply commit with fix to ``sdk-zephyr`` (commit hash: ``10d1197916f81fd8017c2962a88476aba671c773``).
* Unpairing from the device in the :c:member:`bt_conn_cb.security_changed` callback using :c:func:`bt_unpair` function results in an assertion in the Debug configuration or NULL pointer dereference in the Release configuration in the Bluetooth Host keys module.
  This function call is used for rejecting a simultaneous pairing attempt.
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.4.1** tag.

  **Workaround:** Manually port changes with fix to ``sdk-zephyr`` (commit hash ``cd264b21e4a90ed85a63116bd148b890ab347db8`` from the upstream ``zephyr`` repository).
