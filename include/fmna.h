#ifndef FMNA_H_
#define FMNA_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Sound callback structure */
struct fmna_sound_cb {
	/** @brief Request the user to start the play sound action.
	 *
	 *  This callback will be called to start the play sound action.
	 *  The FMN stack will request this action in response to the command
	 *  from the connected peer or in response to the motion detection
	 *  event.
	 *
	 *  The user should notify the FMN stack when the play sound action
	 *  is completed using the @ref fmna_sound_completed_indicate API. If
	 *  the API is not called, the action eventually times out which
	 *  is indicated by the @ref sound_stop callback.
	 */
	void (*sound_start)(void);

	/** @brief Request the user to stop the ongoing play sound action.
	 *
	 *  This callback will be called to stop the ongoing play sound action.
	 *  The FMN stack will request this action in response to the command
	 *  from the connected peer or when the sound event times out before
	 *  the @ref fmna_sound_completed_indicate API is called. The
	 *  @ref fmna_sound_completed_indicate API should not be called after
	 *  the @ref sound_stop callback and will return an error if called.
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
 *  @return Zero on success or negative error code otherwise
 */
int fmna_sound_cb_register(struct fmna_sound_cb *cb);

/** @brief Indicate the completion of the play sound action.
 *
 *  Indicate that the play sound action is completed. This function should be
 *  called only after @ref sound_start callback from @ref fmna_sound_cb structure
 *  was called. This function should not be called if the play sound action is
 *  stopped by the FMN stack. This event is indicated by @ref sound_stop callback
 *  from @ref fmna_sound_cb structure.
 *
 *  @param cb Sound callback structure.
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_sound_completed_indicate(void);

/** @brief Enable serial number lookup.
 *
 *  Enable serial number lookup over BLE for a limited time duration
 *  that is defined in the FMN specification.
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_serial_number_lookup_enable(void);

/** FMN Enable Parameters. */
struct fmna_enable_param {
	/**
	 * @brief Bluetooth identity to be used by the FMN stack.
	 *
	 *  This identity should be created with bt_id_create function that is
	 *  available in the Bluetooth API.
	 *
	 *  @note It is not possible to use the BT_ID_DEFAULT identity for FMN
	 *        because it cannot be combined with bt_id_reset function used
	 *        in the FMN stack.
	 */
	uint8_t bt_id;

	/**
	 * @brief Reset the FMN accessory to default factory settings.
	 *
	 *  This function resets the device to default factory settings as
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
	 *  This callback will be called to indicate that the battery level
	 *  information is requested. The user should provide the battery level
	 *  data with @ref fmna_battery_level_set API in the context of this
	 *  callback. If not provided, the previously set level of the battery
	 *  is used for the current request.
	 *
	 *  @note It is necessary to provide the initial battery level during
	 *  the FMN enabling process (@ref fmna_enable API). This process will
	 *  be aborted with an error if this callback is ignored.
	 */
	void (*battery_level_request)(void);

	/** @brief Notify the user about the exit from the pairing mode.
	 *
	 *  This callback will be called to notify the user about the
	 *  advertising timeout in the pairing mode. It is possible to restart
	 *  the advertising in this mode with the @ref fmna_resume function.
	 *  Such a restart should occur on the explicit intent of the device
	 *  owner (e.g. button press).
	 */
	void (*pairing_mode_exited)(void);
};

/** @brief Set the current battery level as a percentage.
 *
 *  This function sets the current battery level as a percentage. It should
 *  be called in the context of @ref battery_level_request callback from the
 *  @ref fmna_enable_cb structure.
 *
 *  @param percentage_level Battery level as a percentage [0 - 100 %].
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_battery_level_set(uint8_t percentage_level);

/** @brief Resume the activity of the FMN stack.
 *
 *  This function resumes the activity of the FMN stack based on its own
 *  internal state from the last pause. For example, such a pause could occur
 *  after @ref pairing_mode_exited callback from @ref fmna_enable_cb structure.
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_resume(void);

/** @brief Enable the Find My Network (FMN) stack on the accessory.
 *
 *  This function activates the FMN feature. The user should be prepared
 *  to respond to all registered FMN callbacks (e.g. @ref fmna_sound_cb
 *  structure) after calling this API. This function should only be called
 *  after @ref bt_enable function since BLE is required for FMN operations.
 *
 *  @param param Set of parameters to configure the enabling process.
 *  @param cb    Enable callback structure.
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_enable(const struct fmna_enable_param *param,
		const struct fmna_enable_cb *cb);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_H_ */
