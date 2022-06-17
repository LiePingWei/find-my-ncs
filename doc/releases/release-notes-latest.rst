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

* Added support for nRF5340 SoC in the Find My stack and samples.
* Added a dedicated API for turning off the Find My functionality.

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

.. TODO: If you adding new kit to this list, add it also to the release-notes-latest.rst.tmpl

Changelog
*********

* Added a new :c:func:`fmna_disable` function to the Find My API. It disables the FMN stack.
* Added a new :c:func:`fmna_is_ready` function to the Find My API. It checks if the FMN stack is enabled.
* Added a mechanism for activating and deactivating Find My on the long **Button 1** press with the status indication displayed on the **LED 4**.

  This change is available in the Find My Simple and Qualification samples.
* Added a new callback to the Find My API. It notifies the user about the paired state changes.
* Added an indication of the paired state using **LED 3** in the Find My Simple and Qualification samples.
* Pairing mode timeout is no longer restarted on the accessory once a Find My pairing candidate connects to it.
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
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
* The Find My Thingy application does not support the Thingy:53 platform.

.. TODO:
  1. Before the release, make sure that all TODO items in the 'release-notes-latest.rst' file are fulfilled and deleted.
  2. Change ending of the 'release-notes-latest.rst' file name to an actual version, e.g. 'release-notes-1.6.0.rst'.
  3. After the release, copy the 'release-notes-latest.rst.tmpl' file to the 'release-notes-latest.rst'.
