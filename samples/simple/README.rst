.. _simple:

Find My: Simple
###############

Overview
********

The Find My Simple sample demonstrates how to use basic features of the Find My stack like Play Sound and Unwanted Tracking (UT) Detection features.

When the application starts, it enables the Find My stack.
From now on, the accessory is discoverable by the Find My iOS application.

If the accessory is unpaired, you can pair with it using the mobile application.
Note that the FMN advertising in the Unpaired state times out after 10 minutes.
To trigger the advertising again, press **Button 1**.

If the accessory is paired, you can track its location using the Find My application.
You can also use the app interface to play a sound on the device.
**LED 1** on the accessory indicates the play sound action.

To reset the accessory to default factory settings, press **Button 4** during the application boot-up.

The sample uses the background device firmware upgrade (DFU) solution called Unified Accessory Restore Protocol (UARP) which is compatible with the Find My iOS application.

User interface
**************

Button 1:
   Resumes advertising on the accessory when it is unpaired.

Button 2:
   Enables the Serial Number lookup over Bluetooth Low Energy.

Button 3:
   Simulates the motion detection (used with the UT Detection feature). 

Button 4:
   Resets the accessory to default factory settings when pressed during the application boot-up.

LED 1:
   Indicates that the play sound action is in progress.

LED 2:
   Indicates that the motion is detected in the current motion detection period.

Building and running
********************

To build and run this sample, refer to generic instructions from the top-level :file:`README.rst` file.

Testing
=======

After provisioning the MFi tokens to your development kit and programming it, complete the following steps to test the sample:

1. Connect to the kit that runs this sample with a terminal emulator (for example, PuTTY).
   See `How to connect with PuTTY for the required settings <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_testing.html#how-to-connect-with-putty>`_.
#. On your iOS device, pair the accessory using the Find My application:

   a. Open the Find My application.
   #. Navigate to the :guilabel:`Items` tab.
   #. Tap the :guilabel:`Add New Item` button.
   #. In the pop-up window, select the :guilabel:`Other Supported Item` option.

#. Observe that iOS starts to search for FMN items.
#. Tap the :guilabel:`Connect` button once the accessory is found.
#. Complete the FMN pairing process.
#. Select the paired accessory from the item list and tap the :guilabel:`Play Sound` button.
#. Observe that **LED 1** is lit for 5 seconds on the accessory to indicate the play sound action.
#. In the Find My application, tap the :guilabel:`Unpair` button to remove the accessory from the item list.