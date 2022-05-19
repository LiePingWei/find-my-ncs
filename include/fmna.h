/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#ifndef FMNA_H_
#define FMNA_H_

#include <bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup fmna Find My
 * @{
 */

/** @brief Trigger types for a play sound action. */
enum fmna_sound_trigger {
	/** Play sound action is triggered by the Unwanted Tracking Detection
	  * module.
	  */
	FMNA_SOUND_TRIGGER_UT_DETECTION,

	/** Play sound action is triggered by the connected non-owner device. */
	FMNA_SOUND_TRIGGER_NON_OWNER,

	/** Play sound action is triggered by the connected owner device. */
	FMNA_SOUND_TRIGGER_OWNER,
};

/** Sound callback structure */
struct fmna_sound_cb {
	/** @brief Request the user to start the play sound action.
	 *
	 *  This callback is called to start the play sound action.
	 *  The FMN stack requests this action in response to the command
	 *  from the connected peer or in response to the motion detection
	 *  event.
	 *
	 *  The user should notify the FMN stack when the play sound action
	 *  is completed using the @ref fmna_sound_completed_indicate API. If
	 *  the API is not called, the action eventually times out, which
	 *  is indicated by the @ref sound_stop callback.
	 *
	 *  @param sound_trigger Trigger for the play sound action.
	 */
	void (*sound_start)(enum fmna_sound_trigger sound_trigger);

	/** @brief Request the user to stop the ongoing play sound action.
	 *
	 *  This callback is called to stop the ongoing play sound action.
	 *  The FMN stack requests this action in response to the command
	 *  from the connected peer or when the sound event times out before
	 *  the @ref fmna_sound_completed_indicate API is called. The
	 *  @ref fmna_sound_completed_indicate API should not be called after
	 *  the @ref sound_stop callback. It returns an error if called.
	 */
	void (*sound_stop)(void);
};

/** @brief Register sound callbacks.
 *
 *  Register callbacks to handle sound-related activities defined by the FMN
 *  protocol.
 *
 *  @param cb Sound callback structure.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_sound_cb_register(const struct fmna_sound_cb *cb);

/** @brief Indicate the completion of the play sound action.
 *
 *  Indicate that the play sound action has completed. This function should be
 *  called only after the @ref sound_start callback from the @ref fmna_sound_cb
 *  structure is called. This function should not be called if the play sound
 *  action is stopped by the FMN stack. This event is indicated by the 
 *  @ref sound_stop callback from @ref the fmna_sound_cb structure.
 *
 *  @param cb Sound callback structure.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_sound_completed_indicate(void);

/** @brief Motion detection callback structure.
 *
 *  @note All callback functions are executed in the context of the system
 *  clock interrupt handler. The user should use the system workqueue to
 *  perform non-trivial tasks in response to each callback.
 */
struct fmna_motion_detection_cb {
	/** @brief Request the user to start the motion detector.
	 *
	 *  This callback is called to start the motion detection
	 *  activity. From now on, the motion detection events are polled
	 *  periodically with the @ref motion_detection_period_expired API.
	 *  The motion detection activity stops when the
	 *  @ref motion_detection_stop is called.
	 */
	void (*motion_detection_start)(void);

	/** @brief Notify the user that the motion detection period has expired.
	 *
	 *  This callback is called at the end of each
	 *  motion detection period. The @ref motion_detection_start function
	 *  indicates the beginning of the first motion detection period.
	 *  The next period is started as soon as the previous period expires.
	 *  The user should notify the FMN stack if motion was detected
	 *  in the previous period. The return value of this callback
	 *  is used to pass this information.
	 *
	 *  @return true to indicate detected motion in the last period,
	 *  otherwise false.
	 */
	bool (*motion_detection_period_expired)(void);

	/** @brief Notify the user that the motion detector can be stopped.
	 *
	 *  This callback is called to notify the user that the motion
	 *  detector is no longer used by the FMN protocol. It concludes
	 *  the motion detection activity that was started by the
	 *  @ref motion_detection_start callback.
	 */
	void (*motion_detection_stop)(void);
};

/** @brief Register motion detection callbacks.
 *
 *  Register callbacks to handle motion detection activities required by the
 *  Unwanted Tracking (UT) Detection feature from the FMN protocol.
 *
 *  @param cb Motion detection callback structure.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_motion_detection_cb_register(const struct fmna_motion_detection_cb *cb);

/** @brief Enable serial number lookup.
 *
 *  Enable serial number lookup over Bluetooth LE for a limited time
 *  that is defined in the FMN specification.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_serial_number_lookup_enable(void);

/** FMN Enable Parameters. */
struct fmna_enable_param {
	/**
	 * @brief Bluetooth identity to be used by the FMN stack.
	 *
	 *  This identity should be created with the bt_id_create function that is
	 *  available in the Bluetooth API.
	 *
	 *  @note The BT_ID_DEFAULT identity for FMN is not available
	 *        because it cannot be combined with bt_id_reset function used
	 *        in the FMN stack.
	 */
	uint8_t bt_id;

	/**
	 * @brief The initial battery level of the accessory.
	 *
	 *  The battery level is a percentage value set within the inclusive
	 *  range of 0 - 100%.
	 */
	uint8_t init_battery_level;

	/**
	 * @brief Reset the FMN accessory to default factory settings.
	 *
	 *  This flag option resets the device to default factory settings as
	 *  defined by the FMN specification. If the accessory is paired, it
	 *  removes all persistent data that are associated with the owner
	 *  device and the accessory starts to advertise in the unpaired mode.
	 */
	bool use_default_factory_settings;
};

/** FMN Enable callback structure */
struct fmna_enable_cb {
	/** @brief Request the battery level from the user.
	 *
	 *  This callback is called to indicate that the battery level
	 *  information is requested. The user should provide the battery level
	 *  data with @ref fmna_battery_level_set API in the context of this
	 *  callback. If not provided, the previously set level of the battery
	 *  is used for the current request.
	 *
	 *  This callback is optional and can be used to optimize the battery
	 *  level setting operations in the FMN stack. Alternatively, the user
	 *  can ignore this callback and update the battery level periodically
	 *  using @ref fmna_battery_level_set API.
	 */
	void (*battery_level_request)(void);

	/** @brief Indicate the location availability of this accessory to
	 *         other Find My Network devices.
	 *
	 *  This callback is called to indicate whether the location
	 *  of the accessory is available to non-owner devices from the Find My
	 *  Network. This API is intended only for "pair before use" accessories.
	 *  It is used to determine if the "Find My" suffix should be appended to
	 *  the device name for their primary purpose Bluetooth activity
	 *  (for example, advertising or device name GATT characteristic).
	 *
	 *  @note When the accessory is not Find My paired or is connected with
	 *        the Owner device, it is considered Find My Network disabled.
	 *
	 *  @param available True if the accessory is Find My Network enabled.
	 *                   False if the accessory is Find My Network disabled.
	 */
	void (*location_availability_changed)(bool available);

	/** @brief Notify the user about exit from the pairing mode.
	 *
	 *  This callback is called to notify the user about the
	 *  advertising timeout in pairing mode. It is possible to restart
	 *  advertising in this mode with the @ref fmna_resume function.
	 *  Such a restart should occur on the explicit intent of the device
	 *  owner (for example, a button press).
	 */
	void (*pairing_mode_exited)(void);

	/** @brief Indicate paired state changes.
	 *
	 *  This callback is called to indicate that the Find My accessory
	 *  has successfully paired or unpaired.
	 *
	 *  This callback also reports the initial paired state when the user
	 *  enables the FMN stack with the @ref fmna_enable API.
	 *
	 *  @param paired True if the accessory is paired.
	 *                False if the accessory is unpaired.
	 */
	void (*paired_state_changed)(bool paired);
};

/** @brief Set the current battery level as a percentage.
 *
 *  This function sets the current battery level as a percentage. It should
 *  be called in the context of @ref battery_level_request callback from the
 *  @ref fmna_enable_cb structure.
 *
 *  @param percentage_level Battery level as a percentage [0 - 100%].
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_battery_level_set(uint8_t percentage_level);

/** @brief Resume advertising in unpaired mode.
 *
 *  This function resumes advertising in unpaired mode after a timeout.
 *  Such a timeout is indicated by the @ref pairing_mode_exited callback from
 *  the @ref fmna_enable_cb structure.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_resume(void);

/** @brief Enable the Find My Network (FMN) stack on the accessory.
 *
 *  This function activates the FMN feature. The user should be prepared
 *  to respond to all registered FMN callbacks (for example, the 
 *  @ref fmna_sound_cb structure) after calling this API. This function should
 *  only be called after the @ref bt_enable function, because FMN operations
 *  require Bluetooth LE.
 *
 *  @param param Set of parameters to configure the enabling process.
 *  @param cb    Enable callback structure.
 *
 *  @return Zero on success, otherwise a negative error code.
 */
int fmna_enable(const struct fmna_enable_param *param,
		const struct fmna_enable_cb *cb);

/** @brief Disable the Find My Network (FMN) stack on the accessory.
 *
 *  This function deactivates the FMN feature. As a result, all Find My
 *  functionalities like advertising, NFC emulation and key rotation are
 *  stopped. During the disabling process, the accessory also disconnects
 *  all Find My peers that are connected to it over Bluetooth. The disabled
 *  state of the FMN stack is treated similarly to the power-off state.
 *
 *  This function can only be called if the FMN stack was previously enabled
 *  with the @ref fmna_enable API. After the device boot-up, the Find My stack
 *  is disabled.
 *
 *  It is recommended to call this function from the workqueue context to
 *  guarantee a graceful shutdown of the FMN stack without the risk of
 *  interrupting its ongoing operations.
 *
 *  @return Zero on success or negative error code otherwise.
 */
int fmna_disable(void);

/** @brief Check if Find My Network (FMN) stack is ready.
 *
 * @return true when the FMN stack is ready, false otherwise.
 */
bool fmna_is_ready(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /* FMNA_H_ */
