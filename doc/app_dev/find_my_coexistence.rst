.. _find_my_coexistence:

Find My coexistence
###################

Accessories whose primary purpose is *not* to help find items and require a Bluetooth LE pairing before they can be used for their primary purpose are called *pair before use* accessories.
In such devices, the Find My functionality independently coexists with its main Bluetooth LE functionality.
Here are examples of primary purpose applications on such an accessory:

- Heart rate sensor
- LE audio headphones
- HID mouse

The following subsections describe the requirements for enabling the Find My Network accessory protocol on *pair before use* accessories.

You can also refer to the :ref:`coexistence` sample that demonstrates how to implement these requirements.

Identity
********

The primary purpose application should use a dedicated Bluetooth identity that is different from the identity used in the Find My stack.
Distinct Bluetooth identities ensure that main Bluetooth LE functionality and the Find My feature can independently coexist.

Before you enable the stack with the :c:func:`fmna_enable()` function, you need to generate a non-default Bluetooth identity and pass it to the enabling function as an input parameter.
To create such an identity, use the :c:func:`bt_id_create()` function available in the :file:`zephyr/bluetooth.h` header file.
The maximum number of Bluetooth identities you can have is capped and controlled by the ``CONFIG_BT_ID_MAX`` configuration.
It is recommended to use the default identity - ``BT_ID_DEFAULT`` - with the primary purpose application.

Privacy
*******

The primary purpose application should use the Resolvable Private Address (RPA) and periodically rotate its addresses during advertising.
To activate this advertising mode, enable the Privacy feature in the Bluetooth stack by setting the ``CONFIG_BT_PRIVACY`` configuration.
To control the RPA rotation period, use the ``BT_RPA_TIMEOUT`` configuration.
By default, the rotation period is set to 15 minutes.

Even though the ``CONFIG_BT_PRIVACY`` configuration is global, it does not impact the Find My advertising.
In this configuration, the Find My stack keeps advertising according to the Find My specification.

Advertising extension
*********************

To advertise the primary purpose application payload, use the extended advertising API from the :file:`zephyr/bluetooth.h` header file.
When you use this API with default flags, the advertising payload is still visible as legacy advertising to the scanning devices.
It is necessary to use the extended API because it enables two advertising sets to be broadcasted concurrently: the first one with the Find My payload and the second one with the primary application payload.

To control the maximum number of advertising sets, use the ``CONFIG_BT_EXT_ADV_MAX_ADV_SET`` configuration.
Typically, this configuration should be set to two for *pair before use* accessories.

Find My advertising
===================

Find My advertising should be disabled when *pair before use* accessories are connected to a host for its primary purpose.
This behaviour is implemented in the Find My stack and does not require any additional API calls from the user.

Device name
***********

*Pair before use* accessories update their original Bluetooth device name by adding the " - Find My" suffix when the following conditions are met:

- Find My Network is enabled.
- The accessory is in the pairing mode for its primary purpose application.

In all other cases, the device should use its original device name.

You can rely on the ``location_availability_changed`` callback from the ``fmna_enable_cb`` structure to track whether the Find My Network is enabled or disabled.

To dynamically change the device name, use the :c:func:`bt_set_name()` function available in the :file:`zephyr/bluetooth.h` header file and enable the ``CONFIG_BT_DEVICE_NAME_DYNAMIC`` configuration.

Connection filtering
********************

The Bluetooth LE stack in Zephyr supplies connection objects in most of its callbacks.
The connection callbacks API is available in the :file:`zephyr/conn.h` header file.
See the :c:func:`bt_conn_cb_register()` and :c:func:`bt_conn_auth_cb_register()` functions for reference.
Another example of callbacks with connection object parameters is the GATT API.
For reference, see callbacks in the ``bt_gatt_attr`` structure of the :file:`zephyr/gatt.h` header file.

When implementing Bluetooth LE callbacks with the connection object as one of its parameters, you must filter all Find My connections.
Provided that you assigned the ``FMNA_BT_ID`` identity to the FMN stack as the :c:member:`fmna_enable_param.bt_id` parameter in the :c:func:`fmna_enable()` function, you can use the following code template for connection filtering:

   .. code-block:: c

      int err;
      struct bt_conn_info conn_info;

      err = bt_conn_get_info(conn, &conn_info);
      if (err) {
              LOG_ERR("Unable to get connection information and act on it");
              return;
      }

      if (conn_info.id != FMNA_BT_ID) {
              /* You can safely interact in this code scope with connection objects
               * that are not related to the Find My (e.g. HR monitor peer).
               */
      }

This requirement ensures that the primary purpose application logic does not interfere with the Find My activity.
