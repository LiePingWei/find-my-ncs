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
* The Softdevice Controller library incorrectly uses 0 dBm for Find My connection TX power regardless of the :kconfig:option:`CONFIG_FMNA_TX_POWER` Kconfig option value.
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.4.0** tag.
