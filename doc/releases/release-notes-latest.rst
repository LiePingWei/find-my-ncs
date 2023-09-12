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

* Added functions :c:func:`fmna_paired_adv_enable` and :c:func:`fmna_paired_adv_disable` to provide paired advertising management APIs.
  See the API reference documentation for more details.
* Removed the automatic mechanism in the Find My library that disables the Find My paired advertising once a Bluetooth peer connects to the device using Bluetooth identity other than the Find My identity and enables paired advertising once such a peer disconnects.
  The automatic mechanism is replaced by the :c:func:`fmna_paired_adv_enable` and :c:func:`fmna_paired_adv_disable` API functions to provide more flexibility for the developers.
* Aligned the Find My Coexistence sample to use the :c:func:`fmna_paired_adv_enable` and :c:func:`fmna_paired_adv_disable` functions.
* Updated the application development guide for *pair before use* accessories to cover the :c:func:`fmna_paired_adv_enable` and :c:func:`fmna_paired_adv_disable` functions.
* Upgraded the documentation layout to match the standard nRF Connect SDK documentation.
* Changed the bonding flags from "Bonding" to "No Bonding" in the Bluetooth LE Just Works pairing phase, which is the initial step of the Find My pairing flow.
  From now on, all Find My samples and applications perform Bluetooth LE pairing on the Find My Bluetooth identity without storing any bonding information.
  The "No Bonding" mode is particularly important for the pair before use accessories.
  These types of accessories usually bond using their main Bluetooth application identity and prevent the Find My pairing flow in the "Bonding" mode from succeeding.
  In this case, the Find My pairing fails as the Zephyr Bluetooth Host cannot store more than one bond for the same peer (identified by the Identity Address).
* Added the :c:member:`bt_conn_auth_cb.pairing_accept` Bluetooth authentication callback to handle all Bluetooth pairing attempts on the Find My identity.
  It blocks incoming Bluetooth pairing requests on the Find My identity if the device is already Find My paired.
* Added a possibility to disable the Partition Manager module for projects without the UARP DFU configuration.
* Added support for a common target-based DTS configuration in Find My samples.
* Added support for board-specific configurations in the Find My samples.
* Increased the MCUboot partition size in the Debug configuration for all dependent Find My samples and applications.
* Improved the documentation content:

  * Added a new step to the :ref:`ncs_install` page regarding the installation process of the :ref:`cli_tools` package.
  * Improved the installation section in the :ref:`cli_tools` page to cover all supported operating systems.
  * Improved syntax for Kconfig and CMake symbols for all documentation pages.

.. TODO: If there are any changelog entries related to the CLI tools, uncomment the following section and add them to it.
         Otherwise, remove this part of the release notes template.
  CLI Tools
  =========

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
