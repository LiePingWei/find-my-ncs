.. _find_my_release_notes_160:

Find My add-on for nRF Connect SDK v1.6.0
#########################################

.. contents::
   :local:
   :depth: 2

The Find My add-on delivers reference software and libraries for developing Find My accessories in the nRF Connect SDK.

Highlights
**********

The first release covers the following features:

* Support for nRF52832, nRF52833 and nRF52840 SoCs.
* Software and libraries for developing Find My accessories.
* Integration with MCUboot bootloader.
* Support for UARP - a background DFU solution for Find My accessories.
* Simple and Qualification samples.
* PC tool for facilitating the development of Find My accessories.
* Documentation and release notes in RST and HTML formats.

Release tag
***********

The release tag for the Find My add-on for nRF Connect SDK repository is **v1.6.0**.
This release is compatible with the nRF Connect SDK **v1.6.0** tag.

Supported development kits
**************************

* PCA10040 (nRF52832 Development Kit)
* PCA10056 (nRF52840 Development Kit)
* PCA10100 (nRF52833 Development Kit)

Changelog
*********

There are no entries for this section yet.

Limitations
***********

* Documentation might be incomplete.
* NFC is not supported.
* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require waiver for Find My qualification.

Known issues
************

* Find My pairing may sometimes fail due to the connection timeout (especially in the Find My Coexistence sample).
  The root cause of this behaviour is a low value of the link supervision timeout parameter.