.. _thingy:

Find My Thingy
##############

This application showcases the capabilities of Find My using the Thingy onboard peripherals.

Requirements
************

The application supports the following development kits:

+---------------------------------------------------------------------------------------------------------------------------------+-----------+------------------------------+---------+-----------+
|Hardware platform                                                                                                                |PCA        |Build target                  |*ZDebug* |*ZRelease* +
+=================================================================================================================================+===========+==============================+=========+===========+
|:ref:`Thingy:52 <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/boards/arm/thingy52_nrf52832/doc/index.html>`|PCA20020   |``thingy52_nrf52832``         |         | x         |
+---------------------------------------------------------------------------------------------------------------------------------+-----------+------------------------------+---------+-----------+
|:ref:`Thingy:53 <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/boards/arm/thingy53_nrf5340/doc/index.html>` |PCA20053   |``thingy53_nrf5340_cpuapp_ns``| x       | x         |
|                                                                                                                                 |           |``thingy53_nrf5340_cpuapp``   |         |           |
+---------------------------------------------------------------------------------------------------------------------------------+-----------+------------------------------+---------+-----------+

Overview
********

The Find My Thingy application demonstrates how to implement a fully compliant Find My accessory.

The application is based on the :ref:`simple` sample and improves on it by using a speaker and motion detector sensor to implement Find My features.

The speaker is used in the following cases:

* Motion is detected.
* On "Play Sound" command from Find My iOS application.
* After successful action initiated by a button press.

The motion detectors used are the following:

* 9-axis MPU9520 for the Thingy:52.
* BMI270 IMU for the Thingy:53.

The axes used for motion detection are:

* Gyroscope X
* Gyroscope Z

A built-in Analog to digital converter (ADC) measures the battery state.
The battery voltage is divided by the onboard disconnectable voltage divider.
During the measurement, the voltage divider is connected to battery, the ADC measures the voltage and disconnects the voltage divider.

User interface
**************

Button:
   * Short press: Resumes advertising on the accessory when it is unpaired.
   * Medium-length press (>2 s): Enables the serial number lookup over Bluetooth® Low Energy.
   * Long press (>5 s): Resets the accessory to default factory settings.

Speaker/Buzzer:
   Produces sound when the play sound action is in progress.

LED Red:
   Indicates that the initialization is in progress.

LED Green:
   Indicates that the battery level is being measured.

LED Blue:
  Indicates that the motion detection is enabled.

Building and running
********************

To build and run this application, refer to generic instructions in the :ref:`samples_building` section.

Testing
=======

After provisioning the MFi tokens to your development kit and programming it, complete the following steps to test the application:

1. Power on the application.

   **LED Red** should blink shortly.

#. On your iOS device, pair the accessory using the Find My application:

   a. Open the Find My application.
   #. Navigate to the :guilabel:`Items` tab.
   #. Tap the :guilabel:`Add New Item` button.
   #. In the pop-up window, select the :guilabel:`Other Supported Item` option.

#. Observe that iOS starts to search for FMN items.

   **LED Green** should blink indicating the device is measuring the battery level.

#. Tap the :guilabel:`Connect` button once the accessory is found.
#. Select a name and an emoji for your accessory to complete the FMN pairing process.
#. Select the paired accessory from the item list and tap the :guilabel:`Play Sound` button.
#. Observe that the **Speaker** is producing sound for five seconds on the accessory to indicate the play sound action.
#. In the Find My application, tap the :guilabel:`Unpair` button to remove the accessory from the item list.
