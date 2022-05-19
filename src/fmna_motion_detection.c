/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <kernel.h>
#include <string.h>
#include <init.h>

#include "events/fmna_event.h"
#include "fmna_gatt_fmns.h"
#include "fmna_sound.h"
#include "fmna_state.h"

#include <fmna.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(fmna, CONFIG_FMNA_LOG_LEVEL);

#define SEPARATED_UT_TIMER_PERIOD         K_HOURS(24*3)
#define SEPARATED_UT_BACKOFF_PERIOD       K_HOURS(6)
#define SEPARATED_UT_SAMPLING_RATE1       K_SECONDS(10)
#define SEPARATED_UT_SAMPLING_RATE2       K_MSEC(500)
#define SEPARATED_UT_ACTIVE_POLL_DURATION K_SECONDS(20)

#define SEPARATED_UT_MAX_SOUND_COUNT 10

#define TIMER_PERIOD_IS_EQUAL(_running_timer_period, _ref_period) \
	((_running_timer_period.ticks) == (_ref_period.ticks - 1))

/* Time span in separated state before enabling motion detector. */
static k_timeout_t separated_ut_timer_period;

/* Period to disable motion detector if accessory is in separated state */
static k_timeout_t separated_ut_backoff_period;

static const struct fmna_motion_detection_cb *user_cb = NULL;

static void play_sound_work_handle(struct k_work *item);

static void motion_enable_timeout_handle(struct k_timer *timer_id);
static void motion_poll_timeout_handle(struct k_timer *timer_id);
static void motion_poll_duration_timeout_handle(struct k_timer *timer_id);

static K_WORK_DEFINE(play_sound_work, play_sound_work_handle);

static K_TIMER_DEFINE(motion_enable_timer, motion_enable_timeout_handle, NULL);
static K_TIMER_DEFINE(motion_poll_timer, motion_poll_timeout_handle, NULL);
static K_TIMER_DEFINE(motion_poll_duration_timer, motion_poll_duration_timeout_handle, NULL);

static bool motion_detection_enabled;
static bool play_sound_requested;
static uint8_t sound_count;

static void play_sound_work_handle(struct k_work *item)
{
	fmna_sound_start();
}

static void motion_enable_timeout_handle(struct k_timer *timer_id)
{
	LOG_DBG("Enabling the motion detection");

	__ASSERT(user_cb,
		"Motion detection callback structure is not registered. "
		"See fmna_motion_detection_cb_register for details.");
	__ASSERT(user_cb->motion_detection_start,
		"The motion_detection_start callback is not populated. "
		"See fmna_motion_detection_cb_register for details.");

	if (user_cb && user_cb->motion_detection_start) {
		user_cb->motion_detection_start();

		motion_detection_enabled = true;
		k_timer_start(&motion_poll_timer,
			      SEPARATED_UT_SAMPLING_RATE1,
			      SEPARATED_UT_SAMPLING_RATE1);
	} else {
		LOG_ERR("The motion_detection_start callback is not populated");
	}
}

static void state_reset(void)
{
	motion_detection_enabled = false;
	play_sound_requested = false;
	sound_count = 0;

	k_timer_stop(&motion_enable_timer);
	k_timer_stop(&motion_poll_timer);
	k_timer_stop(&motion_poll_duration_timer);
}

static void backoff_setup(void)
{
	LOG_DBG("Setting up motion detection backoff");

	state_reset();
	k_timer_start(&motion_enable_timer, separated_ut_backoff_period, K_NO_WAIT);
}

static void motion_detection_stop(void)
{
	__ASSERT(user_cb,
		"Motion detection callback structure is not registered. "
		"See fmna_motion_detection_cb_register for details.");
	__ASSERT(user_cb->motion_detection_stop,
		"The motion_detection_stop callback is not populated. "
		"See fmna_motion_detection_cb_register for details.");

	if (user_cb && user_cb->motion_detection_stop) {
		user_cb->motion_detection_stop();
	} else {
		LOG_ERR("The motion_detection_stop callback is not populated");
	}
}

static void motion_poll_handle(void)
{
	bool motion_detected;

	__ASSERT(user_cb,
		"Motion detection callback structure is not registered. "
		"See fmna_motion_detection_cb_register for details.");
	__ASSERT(user_cb->motion_detection_period_expired,
		"The motion_detection_period_expired callback is not populated. "
		"See fmna_motion_detection_cb_register for details.");

	if (!user_cb || !user_cb->motion_detection_period_expired) {
		LOG_ERR("The motion_detection_period_expired callback is not populated");
		return;
	}

	motion_detected = user_cb->motion_detection_period_expired();
	if (motion_detected) {
		/* Restart the timer again after a sound playing action. */
		k_timer_stop(&motion_poll_timer);

		/* Switch context for sound playing work. */
		k_work_submit(&play_sound_work);
		play_sound_requested = true;
		sound_count++;

		if (sound_count >= SEPARATED_UT_MAX_SOUND_COUNT) {
			LOG_DBG("Stopping the motion detection: %d sounds played",
				SEPARATED_UT_MAX_SOUND_COUNT);

			backoff_setup();
			motion_detection_stop();
		}
	}
}

static void motion_poll_duration_timeout_handle(struct k_timer *timer_id)
{
	LOG_DBG("Stopping the motion detection: active poll duration timeout");

	backoff_setup();
	motion_detection_stop();
}

static void motion_poll_timeout_handle(struct k_timer *timer_id)
{
	if (TIMER_PERIOD_IS_EQUAL(timer_id->period, SEPARATED_UT_SAMPLING_RATE1)) {
		LOG_DBG("Passive motion polling");

		motion_poll_handle();
	} else if (TIMER_PERIOD_IS_EQUAL(timer_id->period, SEPARATED_UT_SAMPLING_RATE2)) {
		LOG_DBG("Active motion polling");

		motion_poll_handle();
	} else {
		__ASSERT(0, "Misconfigured sampling rate of the motion_poll_timer");
	}
}

int fmna_motion_detection_cb_register(const struct fmna_motion_detection_cb *cb)
{
	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_DETECT_MOTION_ENABLED)) {
		return -ENOTSUP;
	}

	if (user_cb) {
		return -EALREADY;
	}

	if (!cb) {
		return -EINVAL;
	}

	if ((!cb->motion_detection_start) ||
	    (!cb->motion_detection_period_expired) ||
	    (!cb->motion_detection_stop)) {
		return -EINVAL;
	}

	user_cb = cb;
	return 0;
}

static void connected_owner_handle(void)
{
	bool is_enabled;

	is_enabled = motion_detection_enabled;

	state_reset();

	if (is_enabled) {
		LOG_DBG("Stopping the motion detection: owner connected");

		motion_detection_stop();
	} else {
		LOG_DBG("Motion detection is not running: owner connected");
	}
}

static void separated_state_transition_handle(void)
{
	LOG_DBG("Starting the timer for enabling the motion detection");

	k_timer_start(&motion_enable_timer, separated_ut_timer_period, K_NO_WAIT);
}

static void disabled_state_transition_handle(void)
{
	LOG_DBG("Disabling the motion detection");

	/* Stop all kernel objects */
	k_timer_stop(&motion_enable_timer);
	k_timer_stop(&motion_poll_timer);
	k_timer_stop(&motion_poll_duration_timer);

	/* No need to check the return value because
	 * this call is executed in the same context.
	 */
	k_work_cancel(&play_sound_work);

	/* Reset the state. */
	motion_detection_enabled = false;
	play_sound_requested = false;
	sound_count = 0;

	separated_ut_timer_period   = SEPARATED_UT_TIMER_PERIOD;
	separated_ut_backoff_period = SEPARATED_UT_BACKOFF_PERIOD;
}

static void state_transition_handle(void)
{
	enum fmna_state state = fmna_state_get();

	switch (state) {
	case FMNA_STATE_SEPARATED:
		separated_state_transition_handle();
		break;
	case FMNA_STATE_DISABLED:
		disabled_state_transition_handle();
		break;
	default:
		break;
	}
}

static void sound_completed_handle(void)
{
	if (play_sound_requested) {
		play_sound_requested = false;

		if (TIMER_PERIOD_IS_EQUAL(motion_poll_timer.period, SEPARATED_UT_SAMPLING_RATE1)) {
			k_timer_start(&motion_poll_duration_timer,
				      SEPARATED_UT_ACTIVE_POLL_DURATION,
				      K_NO_WAIT);
		}

		k_timer_start(&motion_poll_timer,
			      SEPARATED_UT_SAMPLING_RATE2,
			      SEPARATED_UT_SAMPLING_RATE2);
	}
}

#if CONFIG_FMNA_QUALIFICATION
static void configure_ut_timers_request_handle(struct bt_conn *conn,
					       uint32_t separated_ut_timeout,
					       uint32_t separated_ut_backoff)
{
	int err;
	uint16_t resp_opcode;

	LOG_INF("FMN Debug CP: responding to configure UT timers request:");
	LOG_INF("Separated UT timeout: %d [s]", separated_ut_timeout);
	LOG_INF("Separated UT backoff: %d [s]", separated_ut_backoff);

	separated_ut_timer_period = K_SECONDS(separated_ut_timeout);
	separated_ut_backoff_period = K_SECONDS(separated_ut_backoff);

	resp_opcode = fmna_debug_event_to_gatt_cmd_opcode(FMNA_DEBUG_EVENT_CONFIGURE_UT_TIMERS);
	FMNA_GATT_COMMAND_RESPONSE_BUILD(resp_buf, resp_opcode, FMNA_GATT_RESPONSE_STATUS_SUCCESS);
	err = fmna_gatt_debug_cp_indicate(conn, FMNA_GATT_DEBUG_COMMAND_RESPONSE_IND, &resp_buf);
	if (err) {
		LOG_ERR("fmna_gatt_debug_cp_indicate returned error: %d", err);
	}
}
#endif

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (!IS_ENABLED(CONFIG_FMNA_CAPABILITY_DETECT_MOTION_ENABLED)) {
		return false;
	}

	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		switch (event->id) {
		case FMNA_EVENT_OWNER_CONNECTED:
			connected_owner_handle();
			break;
		case FMNA_EVENT_SOUND_COMPLETED:
			sound_completed_handle();
			break;
		case FMNA_EVENT_STATE_CHANGED:
			state_transition_handle();
			break;
		default:
			break;
		}

		return false;
	}

#if CONFIG_FMNA_QUALIFICATION
	if (is_fmna_debug_event(aeh)) {
		struct fmna_debug_event *event = cast_fmna_debug_event(aeh);

		switch (event->id) {
		case FMNA_DEBUG_EVENT_CONFIGURE_UT_TIMERS:
			configure_ut_timers_request_handle(
				event->conn,
				event->configure_ut_timers.separated_ut_timeout,
				event->configure_ut_timers.separated_ut_backoff);
			break;
		default:
			break;
		}

		return false;
	}
#endif

	return false;
}

APP_EVENT_LISTENER(fmna_motion_detection, app_event_handler);
APP_EVENT_SUBSCRIBE(fmna_motion_detection, fmna_event);

#if CONFIG_FMNA_QUALIFICATION
APP_EVENT_SUBSCRIBE(fmna_motion_detection, fmna_debug_event);
#endif

static int motion_detection_init(const struct device *unused)
{
	if (IS_ENABLED(CONFIG_FMNA_CAPABILITY_DETECT_MOTION_ENABLED)) {
		separated_ut_timer_period   = SEPARATED_UT_TIMER_PERIOD;
		separated_ut_backoff_period = SEPARATED_UT_BACKOFF_PERIOD;
	}

	return 0;
}

SYS_INIT(motion_detection_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
