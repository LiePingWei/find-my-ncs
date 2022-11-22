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

This release covers the following features:

* Added support for the Thingy:53 board in the Find My Thingy application.

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

* The Find My library uses the pure Oberon library for Find My cryptographic operations
  when the nrfxlib Security module is not enabled and the RNG peripheral is available in the CPU image.
  This new default configuration frees around 4 kB of flash memory.
* By default, the Find My accessory requests longer supervision timeout in the Connection Parameter Update procedure.
  The new timeout value reduces disconnection probability due to the connection timeout.
* Aligned references to all public Zephyr headers that have been moved to the :file:`include/zephyr` folder.
* Fixed the return value check of Bluetooth identity creation in the Find My Thingy application.
* Improved RTT logging (bigger RTT buffer) of the Debug configuration in Find My projects.
* Added motion and speaker platform abstraction in the Find My application.
  Platform implementations now support both Thingy:53 and Thingy:52.
* In the Find My Coexistence sample, added a mechanism for activating and deactivating Find My on a long **Button 1** press with the status indication displayed on the **LED 4**.
* In the Find My Connection module, fixed the :c:func:`bt_conn_foreach` API calls that are used to iterate over connected peers.
* Added a new :kconfig:option:`CONFIG_FMNA_SERVICE_HIDDEN_MODE` configuration option for hiding Find My services in the disabled state of the FMN stack.
* Enabled the :kconfig:option:`CONFIG_FMNA_SERVICE_HIDDEN_MODE` configuration option in the Find My Coexistence sample.
* Added a new callback to the Find My API. It notifies the user about the Find My pairing failure.
* Added a log indication of the Find My pairing failure to the Find My Simple and Qualification samples.
* The FMN stack advertises with a new Bluetooth address in case of a Find My pairing failure to work around the iOS issue with the cached bond information.
* Fixed an issue with the Unpair command rejection when the command was used in the same connection session as the Find My pairing.
  The issue caused the FMN stack to persist in the paired state when the Find My app user cancelled the pairing flow after a successful pairing packet exchange.
* All accessory parameters that are configurable by iOS are set to their default values after the Unpair procedure.

CLI Tools
=========

* Fixed the :ref:`super_on_github` sample on Windows platform.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3dBm, violating the Find My specification requirement for 4dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.

.. TODO:
  1. Before the release, make sure that all TODO items in the 'release-notes-latest.rst' file are fulfilled and deleted.
  2. Change ending of the 'release-notes-latest.rst' file name to an actual version, e.g. 'release-notes-1.6.0.rst'.
  3. After the release, copy the 'release-notes-latest.rst.tmpl' file to the 'release-notes-latest.rst'.
