.. _find_my_release_notes_241:

Find My add-on for nRF Connect SDK v2.4.1
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

The release tag for the Find My add-on for nRF Connect SDK repository is **v2.4.1**.
This release is compatible with nRF Connect SDK **v2.4.1** tag.

Installing
**********

Follow the instructions in the :ref:`ncs_install` document.
Run the following command to install this specific release version:

.. code-block:: console

    west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.4.1

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

* Fixed an invalid timer period comparison in the Motion Detection module, which caused Unwanted Tracking Detection to work incorrectly and crash the application due to the unsatisfied assert statement.
  A fix introduced in the Zephyr kernel changed the way the timer period is set and it was necessary to align the Motion Detection module with this change.
* Fixed an issue with the :c:func:`bt_unpair` function call in the :c:member:`bt_conn_cb.security_changed` callback of the Find My Pairing module.
  This operation resulted in an assertion in the Debug configuration or NULL pointer dereference in the Release configuration in the Bluetooth Host keys module.
  This function call is used for rejecting a simultaneous pairing attempt.

Known issues and limitations
****************************

* nRF52832 and nRF52833 SoCs are only supported in the Release configuration due to memory limitations.
* nRF5340 SoC supports a maximum transmit power of 3 dBm, violating the Find My specification requirement for 4 dBm.
* Find My characteristics are always available regardless of the accessory state.
  This limitation will require a waiver for Find My qualification.
* Firmware updates of the nRF5340 network core are not supported with the UARP protocol.
* Find My pairing is rejected during the Just Works Bluetooth LE pairing phase if the device is already bonded with the same peer on any Bluetooth identity.
  The issue can be reproduced in the case of the pair before use accessories.
  These types of accessories usually bond using their main Bluetooth application identity and prevent the Find My pairing flow in the "Bonding" mode from succeeding.
  In this case, the Find My pairing fails as the Zephyr Bluetooth Host cannot store more than one bond for the same peer (identified by the Identity Address).
  The issue is fixed on the nRF Connect SDK **main** branch and in all releases beginning from the **v2.5.0** tag.

  **Workaround:** Manually cherry-pick and apply commits with fixes to:

  * ``sdk-zephyr`` (commit hash ``9a0b8a317a91089f048c38233635240f21ab298d``)
  * ``sdk-find-my`` (commit hash ``36e75564d83b2d2068e08d9d9df7cbfedd668cb6``)
