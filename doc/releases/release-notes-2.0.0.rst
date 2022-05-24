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
