.. _find_my_release_notes_latest:

Find My add-on for nRF Connect SDK v1.6.99
##########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Support for *pair before use* accessories, also called coexistence feature.
* NFC support using the NFC-A Type 2 Tag library.
* A new sample for the SuperBinary tool that can create a SuperBinary file
  remotely using GitHub Actions.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v1.6.99**.
This release is compatible with nRF Connect SDK **v1.6.99** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v1.6.99

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)

Changelog
*********

* Added the Coexistence sample that demonstrates how to create firmware for *pair before use* accessories.
* Added support for *pair before use* accessories in the Find My stack.
* Added new API in the Find My header file for *pair before use* accessories.
* Added conceptual docs for *pair before use* accessories.
* Added conceptual docs for firmware update with UARP.
* Added NFC support to the Find My stack using the NFC-A Type 2 Tag library.
* Enabled NFC support in the Find My Simple and Qualification samples.
* Added logic for removing Bluetooth LE bond information of a peer that does not finish the Find My pairing procedure.
* Added support for custom serial numbers from provisioned data.
* Added option to configure transmit power used in Find My advertising and connections.
* Fixed the advertising timeout after disconnect in the persistent connection mode.
* Increased the MCUboot partition size for the release variant in affected samples to support nRF52840 target.
* Providing version information is optional for a SuperBinary tool,
  because it can read the version from the source MCUBoot image.

Limitations
***********

There are no entries for this section yet.

Known issues
************

There are no entries for this section yet.
