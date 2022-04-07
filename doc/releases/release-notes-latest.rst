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
* PCA20020 (Thingy:52 Prototyping Platform)

.. TODO: If you adding new kit to this list, add it also to the release-notes-latest.rst.tmpl

Changelog
*********

* Fixed the serial number counter to start from zero instead of one.
* Fixed an array overwrite when the serial number string is 16 bytes long.
* Populated unset memory regions of variables that are used during serial number encryption.
* Fixed out-of-bounds access for crypto key derivation operation.

  The access was triggered during the error handling exit from the function.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.

.. TODO:
  1. Before the release, make sure that all TODO items in the 'release-notes-latest.rst' file are fulfilled and deleted.
  2. Change ending of the 'release-notes-latest.rst' file name to an actual version, e.g. 'release-notes-1.6.0.rst'.
  3. After the release, copy the 'release-notes-latest.rst.tmpl' file to the 'release-notes-latest.rst'.
