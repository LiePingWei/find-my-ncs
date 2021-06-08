.. _coexistence:

Find My Coexistence
###################

Overview
********

The Find My Coexistence sample demonstrates how to set up a *pair before use* accessory whose primary purpose is not to help find items.
In such an accessory, the Find My functionality independently coexists with its main Bluetooth LE functionality.
In this sample, the primary purpose application is the Heart Rate (HR) sensor.

When the application starts, it enables the Find My stack.
From now on, the accessory is discoverable by the Find My iOS application.
The Find My stack is configured to support the following capabilities:

- play sound
- serial number lookup over Bluetooth LE

When the application starts, it also starts the HR sensor advertising.
The HR sensor uses the Privacy feature of the Bluetooth LE stack and periodically rotates its address during advertising.

To set the HR sensor in the pairing mode, press **Button 3**.
Note that the accessory must be in the pairing mode during the first connection with an unknown HR monitor device.

When the HR monitor connects to the accessory, it should encrypt and authenticate the link using either the Passkey pairing method or an already existing bond.
This is necessary to interact with the HR service and all of its characteristics.
The access to Heart Rate data is restricted and only available to the authenticated peers on an encrypted connection.

Once the accessory has paired successfully, it will exit the pairing mode automatically.
To exit the pairing mode manually at any time, press **Button 3** again.

To remove all HR sensor bonds, press **Button 4** during the application bootup.
This action also resets the Find My stack to the default factory settings.

In this application, the Bluetooth LE device name can change in the runtime.
If the HR sensor application is in the pairing mode and the Find My Network is enabled, the "- Find My" suffix is appended to the base name.
In all other cases, the suffix is not added to the Bluetooth LE device name.

Requirements
************

The sample supports the following development kits:

+-------------------+-----------+--------------------+---------+-----------+
|Hardware platforms |PCA        |Build target        |*ZDebug* |*ZRelease* +
+===================+===========+====================+=========+===========+
|nRF52840 DK        |PCA10056   |nrf52840dk_nrf52840 | x       | x         |
+-------------------+-----------+--------------------+---------+-----------+
|nRF52833 DK        |PCA10100   |nrf52833dk_nrf52833 | x       | x         |
+-------------------+-----------+--------------------+---------+-----------+
|nRF52 DK           |PCA10040   |nrf52dk_nrf52832    | x       | x         |
+-------------------+-----------+--------------------+---------+-----------+

User interface
**************

Button 1:
   Resumes advertising on the accessory when it is unpaired.

Button 2:
   Enables the Serial Number lookup over Bluetooth LE.

Button 3:
   Enables/disables pairing mode for the HR Sensor.

Button 4:
   Decrements the battery level.
   Resets the accessory to default factory settings when pressed during the application bootup.

LED 1:
   Indicates that the play sound action is in progress.

Building and running
********************

To build and run this sample, refer to the generic instructions in the :ref:`samples_building` section.

Testing
=======

After provisioning the MFi tokens to your development kit and programming it, complete the following steps to test the sample:

1. Connect to the kit that runs this sample with a terminal emulator (for example PuTTY).
   See `How to connect with PuTTY for the required settings <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_testing.html#how-to-connect-with-putty>`_.
#. On your iOS device, pair the accessory using the Find My application:

   a. Open the Find My application.
   #. Navigate to the :guilabel:`Items` tab.
   #. Tap the :guilabel:`Add New Item` button.
   #. In the pop-up window, select the :guilabel:`Other Supported Item` option.

#. Observe that iOS starts to search for FMN items.
#. Tap the :guilabel:`Connect` button once the accessory is found.
#. Complete the FMN pairing process.
#. Press **Button 3** to set the HR sensor on the accessory in the pairing mode.
#. Open the nRF Connect iOS application.
#. Connect to the "HR Sensor - Find My" device.

.. note::
   If the Find My owner has already connected to the accessory, the " - Find My" suffix will be missing.

#. Select the :guilabel:`Client` tab and scroll down to find Heart Rate service characteristics.
#. Try reading the Body Sensor Location characteristic and observe that the pairing window pops up.
#. Enter the passkey that is displayed in the firmware logs and complete the pairing procedure.
#. Enable the Heart Rate Measurement notifications and observe that the Heart Rate value changes every second.
#. Tap the :guilabel:`Disconnect` button and then the :guilabel:`Close` button
#. Open the Find My iOS application again.
#. Select the paired accessory from the item list and tap the :guilabel:`Play Sound` button.
#. Observe that **LED 1** is lit for five seconds on the device to indicate the play sound action.
#. Go back to the nRF Connect iOS application.
#. Scroll down to refresh the scanning process.
#. Connect to the "HR Sensor".
#. Observe that the connection security is upgraded to level 4.
#. Find the Heart Rate Measurement characteristic again, enable it and observe that the Heart Rate value changes every second.
#. Switch back to the Find My iOS application without triggering the disconnect for the HR Sensor in the nRF Connect.
#. Play sound again.
#. Observe that Find My and HR Sensor links are maintained at the same time.
