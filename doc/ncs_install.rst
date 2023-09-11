.. _ncs_install:

Installing the nRF Connect SDK
##############################

Use the west tool to install all components of the nRF Connect SDK, including the Find My add-on.
The required version of west is 0.10.0 or higher.

1. Follow the installation instructions in `the nRF Connect SDK Installation guide <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation.html>`_.
#. Enable the Find My group filter:

   .. code-block:: console

      west config manifest.group-filter +find-my

   .. note::
      For more information about how west groups work, see `west groups documentation <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/develop/west/manifest.html#project-groups>`_.

#. Update west to download the Find My repository:

   .. code-block:: console

      west update
      // find-my repository is downloaded
      // your github access will be checked
