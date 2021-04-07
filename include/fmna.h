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
	 *  is completed using the fmna_sound_completed_indicate API. If the
	 *  API is not called, the action eventually times out which
	 *  is indicated by the sound_stop callback.
	 */
	void (*sound_start)(void);

	/** @brief Request the user to stop the ongoing play sound action.
	 *
	 *  This callback will be called to stop the ongoing play sound action.
	 *  The FMN stack will request this action in response to the command
	 *  from the connected peer or when the sound event times out before
	 *  the fmna_sound_completed_indicate API is called. The
	 *  fmna_sound_completed_indicate API should not be called after the 
	 *  sound_stop callback and will return an error if called.
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
 *  called only after sound_start callback from fmna_sound_cb structure was
 *  called. This function should not be called if the play sound action is
 *  stopped by the FMN stack. This event is indicated by sound_stop callback
 *  from fmna_sound_cb structure.
 *
 *  @param cb Sound callback structure.
 *
 *  @return Zero on success or negative error code otherwise
 */
int fmna_sound_completed_indicate(void);


struct fmna_init_params {
	uint8_t bt_id;
	bool use_default_factory_settings;
};

int fmna_init(const struct fmna_init_params *init_params);

int fmna_serial_number_lookup_enable(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_H_ */