.. _find_my_release_notes_190:

Find My add-on for nRF Connect SDK v1.9.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

This release covers the following features:

* Added Find My Thingy application with the Thingy:52 board support


Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v1.9.0**.
This release is compatible with nRF Connect SDK **v1.9.0** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v1.9.0

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)
* PCA20020 (Thingy:52 Prototyping Platform)

Changelog
*********

* Changed the logging format in the Debug variant for all Find My samples.
* Enabled RTT as an additional logging backend in the Debug variant for all Find My samples.
* Aligned Find My NFC implementation with the NFC library API changes from nrfxlib.
* Added support for the Thingy:52 board
* Created applications directory with the Find My Thingy application inside it.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Find My pairing may sometimes fail due to the connection timeout (especially in the Find My Coexistence sample).
  The root cause of this behaviour is a low value of the link supervision timeout parameter.