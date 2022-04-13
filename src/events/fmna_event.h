#ifndef FMNA_EVENT_H_
#define FMNA_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "fmna_keys.h"

#include "app_event_manager.h"

#include <bluetooth/conn.h>

#define FMNA_EVENT_CREATE(_name, _evt_id, _conn)     \
	struct fmna_event *_name = new_fmna_event(); \
	_name->id = _evt_id;			     \
	_name->conn = _conn;

enum fmna_event_id {
	FMNA_EVENT_BATTERY_LEVEL_CHANGED,
	FMNA_EVENT_BONDED,
	FMNA_EVENT_MAX_CONN_CHANGED,
	FMNA_EVENT_OWNER_CONNECTED,
	FMNA_EVENT_PAIRING_COMPLETED,
	FMNA_EVENT_PEER_CONNECTED,
	FMNA_EVENT_PEER_DISCONNECTED,
	FMNA_EVENT_PEER_SECURITY_CHANGED,
	FMNA_EVENT_PUBLIC_KEYS_CHANGED,
	FMNA_EVENT_SERIAL_NUMBER_CNT_CHANGED,
	FMNA_EVENT_SOUND_COMPLETED,
	FMNA_EVENT_STATE_CHANGED,
};

struct fmna_public_keys_changed {
	bool separated_key_changed;
};

struct fmna_peer_security_changed {
	enum bt_security_err err;
	bt_security_t level;
};

struct fmna_event {
	struct app_event_header header;

	enum fmna_event_id id;
	struct bt_conn *conn;
	union {
		struct fmna_public_keys_changed public_keys_changed;
		struct fmna_peer_security_changed peer_security_changed;
	};
};

APP_EVENT_TYPE_DECLARE(fmna_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_EVENT_H_ */
