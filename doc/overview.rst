.. _overview:

Overview
########

.. contents::
   :local:
   :depth: 2

The nRF Connect SDK, together with this Find My add-on and Nordic Semiconductor SoCs, provide a single-chip Bluetooth Low Energy accessory solution that fulfills the requirements defined by the MFi program for the Find My Network (FMN).

nRF Connect SDK
***************

The nRF Connect SDK is hosted, distributed, and supported by Nordic Semiconductor for product development with Nordic SoCs.
The SDK is distributed in multiple Git repositories with application code, drivers, re-distributable libraries, and forks of open-source code used in the SDK.
This add-on is also a part of nRF Connect SDK and requires other SDK repositories to work correctly.
Use the west tool to set up this distributed codebase and to keep it in sync based on the manifest file - :file:`west.yml` from `the main SDK repository <https://github.com/nrfconnect/sdk-nrf>`_.:

For more information about the SDK, see `the nRF Connect SDK documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html>`_.
