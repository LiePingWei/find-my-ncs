#ifndef FMNA_EVENT_H_
#define FMNA_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "fmna_keys.h"

#include "event_manager.h"

#include <bluetooth/conn.h>

#define FMNA_EVENT_CREATE(_name, _evt_id, _conn)     \
	struct fmna_event *_name = new_fmna_event(); \
	_name->id = _evt_id;			     \
	_name->conn = _conn;

enum fmna_event_id {
	FMNA_OWNER_CONNECTED,
	FMNA_PAIRING_COMPLETED,
	FMNA_PUBLIC_KEYS_CHANGED,
};

struct fmna_public_keys_changed {
	bool secondary_key_changed;
};

struct fmna_event {
	struct event_header header;

	enum fmna_event_id id;
	struct bt_conn *conn;
	union {
		struct fmna_public_keys_changed public_keys_changed;
	};
};

EVENT_TYPE_DECLARE(fmna_event);

#ifdef __cplusplus
}
#endif

#endif /* FMNA_EVENT_H_ */