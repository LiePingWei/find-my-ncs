/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/net/buf.h>
#include <dfu/dfu_target.h>
#include <dfu/dfu_target_mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/dfu/mcuboot.h>
#include <ocrypto_sha256.h>

#include <zephyr/logging/log.h>
#define LOG_MODULE_NAME fmna_uarp

#include "events/fmna_event.h"

#include "CoreUARPPlatform.h"
#include "CoreUARPUtils.h"
#include "CoreUARPAccessory.h"
#include "CoreUARPProtocolDefines.h"
#include "CoreUARPPlatformAccessory.h"

#include "fmna_uarp.h"
#include "fmna_serial_number.h"
#include "fmna_version.h"

LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_FMNA_UARP_LOG_LEVEL);

BUILD_ASSERT(!IS_ENABLED(DFU_TARGET_STREAM_SAVE_PROGRESS),
	     "FMNA UARP does not support DFU target progress saving.");

#define TLV_TYPE_SHA2          0xF4CE36FEuL
#define TLV_TYPE_APPLY_FLAGS   0xF4CE36FCuL
#define APPLY_FLAGS_FAST_RESET 0xFF

#define TX_MESSAGE_HEADROOM_SIZE 1
#define MAX_TX_MESSAGE_SIZE      (CONFIG_FMNA_UARP_TX_MSG_PAYLOAD_SIZE + sizeof(union UARPMessages))

#define PAYLOAD_4CC_LENGTH 5

enum last_error_code {
	LAST_ERROR_UNSET                           = 0,
	LAST_ERROR_NONE                            = 1,
	LAST_ERROR_ASSET_REQUEST_METADATA_FAILED   = 2,
	LAST_ERROR_ASSET_SET_PAYLOAD_INDEX_FAILED  = 3,
	LAST_ERROR_PAYLOAD_REQUEST_METADATA_FAILED = 4,
	LAST_ERROR_NO_APPLICABLE_PAYLOAD           = 5,
	LAST_ERROR_INVALID_HASH_TLV_LENGTH         = 6,
	LAST_ERROR_INVALID_APPLY_FLAGS_TLV_LENGTH  = 7,
	LAST_ERROR_DFU_TARGET_INIT_FAILED          = 8,
	LAST_ERROR_PAYLOAD_REQUEST_DATA_FAILED     = 9,
	LAST_ERROR_IMAGE_WRITE_FAILED              = 10,
	LAST_ERROR_DFU_DONE_FAILED                 = 11,
	LAST_ERROR_INVALID_HASH                    = 12,
	LAST_ERROR_ASSET_FULLY_STAGED_FAILED       = 13,
	LAST_ERROR_ASSET_ACCEPT_FAILED             = 14,
};

enum asset_state {
	ASSET_NONE = 0,
	ASSET_ACTIVE,
	ASSET_ORPHANED,
	ASSET_STAGED,
	ASSET_APPLIED,
	ASSET_FAILED,
};

static struct fmna_uarp_accessory
{
	struct uarpPlatformAccessory accessory;
	struct uarpPlatformController controller;
	struct uarpPlatformAsset *asset;
	struct UARPVersion payload_version;
	struct net_buf_simple *buf;
	struct net_buf_simple *pending_buf;
	ocrypto_sha256_ctx hash_ctx;
	fmna_uarp_send_message_fn send_message;
	uint32_t last_error;
	enum asset_state state;
	uint8_t payload_hash[ocrypto_sha256_BYTES];
	uint8_t apply_flags;
	bool dfu_target_init_done;
} accessory;

static uint32_t query_active_firmware_version(void *accessory_delegate,
					      uint32_t asset_tag,
					      struct UARPVersion *version);
static void payload_meta_data_complete(void *accessory_delegate, void *asset_delegate);
static void asset_meta_data_complete(void *accessory_delegate, void *asset_delegate);

void fmna_uarp_controller_add(void)
{
	uint32_t status;

	LOG_INF("Adding controller");
	
	status = uarpPlatformControllerAdd(&accessory.accessory,
					   &accessory.controller,
					   (void *)&accessory.controller);

	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformControllerAdd failed, status 0x%04X", status);
	}
}

void fmna_uarp_controller_remove(void)
{
	uint32_t status;
	
	LOG_INF("Removing controller");

	uarpFree(accessory.buf);
	uarpFree(accessory.pending_buf);
	accessory.buf = NULL;
	accessory.pending_buf = NULL;

	status = uarpPlatformControllerRemove(&accessory.accessory, &accessory.controller);

	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformControllerRemove failed, status 0x%04X", status);
	}
}

void fmna_uarp_recv_message(struct net_buf_simple *buf)
{
	uint32_t status;

	status = uarpPlatformAccessoryRecvMessage(&accessory.accessory,
						  &accessory.controller,
						  buf->data,
						  buf->len);
	
	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformAccessoryRecvMessage failed, status 0x%04X", status);
	}
}

static uint32_t request_buffer(void *accessory_delegate, uint8_t **buffer, uint32_t bufferLength)
{
	__ASSERT(buffer, "NULL argument");

	*buffer = uarpZalloc(bufferLength);

	if (*buffer == NULL) {
		LOG_ERR("Out of heap memory");
		return kUARPStatusNoResources;
	}

	return kUARPStatusSuccess;
}

static void return_buffer(void *accessory_delegate, uint8_t *buffer)
{
	uarpFree(buffer);
}

static struct net_buf_simple *net_buf_simple_from_uarp_buffer(uint8_t *buffer, uint32_t length)
{
	struct net_buf_simple *buf = (struct net_buf_simple *) (buffer - TX_MESSAGE_HEADROOM_SIZE -
							        sizeof(struct net_buf_simple));

	if (buffer) {
		buf->data = buffer;
		buf->len = length;
		buf->size = TX_MESSAGE_HEADROOM_SIZE + MAX_TX_MESSAGE_SIZE;
		buf->__buf = buffer - TX_MESSAGE_HEADROOM_SIZE;
		return buf;
	} else {
		return NULL;
	}
}

static uint8_t *net_buf_simple_to_uarp_buffer(struct net_buf_simple *buf)
{
	return (uint8_t *) buf + sizeof(struct net_buf_simple) + TX_MESSAGE_HEADROOM_SIZE;
}

static uint32_t request_transmit_msg_buffer(void *accessory_delegate,
					    void *controller_delegate,
					    uint8_t **buffer,
					    uint32_t *length)
{
	struct net_buf_simple *buf;

	__ASSERT(buffer, "NULL argument");
	__ASSERT(length, "NULL argument");

	*length = MAX_TX_MESSAGE_SIZE;

	buf = uarpZalloc(sizeof(struct net_buf_simple) + TX_MESSAGE_HEADROOM_SIZE + MAX_TX_MESSAGE_SIZE);

	if (buf == NULL) {
		*buffer = NULL;
		LOG_ERR("Out of heap memory");
		return kUARPStatusNoResources;
	}
	
	*buffer = net_buf_simple_to_uarp_buffer(buf);
	return kUARPStatusSuccess;
}

static void return_transmit_msg_buffer(void *accessory_delegate,
				void *controller_delegate,
				uint8_t *buffer)
{
	uarpFree(net_buf_simple_from_uarp_buffer(buffer, 0));
}

static uint32_t send_message(void *accessory_delegate,
			     void *controller_delegate,
			     uint8_t *buffer,
			     uint32_t length)
{
	struct net_buf_simple *buf;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;

	__ASSERT(accessory_delegate, "NULL argument");
	__ASSERT(buffer, "NULL argument");
	__ASSERT(0 < length && length <= MAX_TX_MESSAGE_SIZE, "Invalid length");
	
	buf = net_buf_simple_from_uarp_buffer(buffer, length);

	if (accessory->buf == NULL) {
		accessory->buf = buf;
		return accessory->send_message(buf);
	} else if (accessory->pending_buf == NULL) {
		accessory->pending_buf = buf;
		return kUARPStatusSuccess;
	} else {
		LOG_ERR("Already have a pending UARP TX");
		return kUARPStatusNoResources;
	}
}

void fmna_uarp_send_message_complete(void)
{
	struct net_buf_simple *buf;

	__ASSERT(accessory.buf, "Invalid state");

	buf = accessory.buf;

	if (accessory.pending_buf) {
		accessory.buf = accessory.pending_buf;
		accessory.pending_buf = NULL;
	} else {
		accessory.buf = NULL;
	}

	uarpPlatformAccessorySendMessageComplete(&accessory.accessory,
						 &accessory.controller,
						 net_buf_simple_to_uarp_buffer(buf));
	
	if (accessory.buf) {
		accessory.send_message(accessory.buf);
	}
}

static uint32_t data_transfer_pause(void *accessory_delegate, void *p_controller_delegate)
{
	LOG_INF("Transfer paused by the controller");
	return kUARPStatusSuccess;
}

static uint32_t data_transfer_resume(void *accessory_delegate, void *p_controller_delegate)
{
	LOG_INF("Transfer resumed by the controller");
	return kUARPStatusSuccess;
}

static void super_binary_offered(void *accessory_delegate,
				 void *controller_delegate,
				 struct uarpPlatformAsset *asset)
{
	uint32_t status;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	UARPBool is_acceptable;

	__ASSERT(accessory_delegate, "NULL argument");
	__ASSERT(controller_delegate, "NULL argument");
	__ASSERT(asset, "NULL argument");
	
	LOG_INF("Asset Offered <%08x> <Version %u.%u.%u.%u>",
		asset->core.assetTag,
		asset->core.assetVersion.major,
		asset->core.assetVersion.minor,
		asset->core.assetVersion.release,
		asset->core.assetVersion.build);

	status = uarpPlatformAccessoryAssetIsAcceptable(&accessory->accessory,
							asset,
							&is_acceptable);
	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformAccessoryAssetIsAcceptable failed with status %d", status);
		is_acceptable = kUARPNo;
	}

	if (is_acceptable == kUARPNo) {

		LOG_INF("Asset is not acceptable");

	} else if (!uarpAssetIsSuperBinary(&asset->core)) {

		/* Dynamic assets are not acceptable */
		is_acceptable = kUARPNo;

	} else if (accessory->state == ASSET_NONE) {

		/* Accept acceptable asset if there is nothing else currently */
		LOG_INF("Asset is acceptable");

	} else if (accessory->state == ASSET_ORPHANED) {

		if (uarpAssetCoreCompare(&accessory->asset->core, &asset->core) ==
		    kUARPVersionComparisonResultIsEqual) {

			LOG_INF("Merging offered SuperBinary and orphaned SuperBinary");
			status = uarpPlatformAccessorySuperBinaryMerge(&accessory->accessory,
								       accessory->asset,
								       asset);
			if (status != kUARPStatusSuccess) {
				LOG_ERR("uarpPlatformAccessorySuperBinaryMerge failed"
					" with status %d", status);
				is_acceptable = kUARPNo;
			} else {
				asset = accessory->asset;
			}

		} else {

			LOG_INF("Accepting offered and abandoning orphaned SuperBinary");
			uarpPlatformAccessoryAssetAbandon(&accessory->accessory,
							  NULL,
							  accessory->asset);
		}

	} else {
		
		/* UARP service allows just one controller, so there is no need for competing
		 * controllers handling. Deny new asset in all other states. */
		is_acceptable = kUARPNo;

	}

	if (is_acceptable == kUARPYes) {

		accessory->asset = asset;
		accessory->state = ASSET_ACTIVE;
		asset->pDelegate = asset;

		status = uarpPlatformAccessoryAssetAccept(&accessory->accessory,
							  &accessory->controller,
							  asset);
		if (status != kUARPStatusSuccess) {
			uarpPlatformAccessoryAssetRelease(&accessory->accessory,
							  &accessory->controller,
							  asset);
			accessory->asset = NULL;
			accessory->state = ASSET_NONE;
			accessory->last_error = (LAST_ERROR_ASSET_ACCEPT_FAILED << 16) |
						(status & 0xFFFF);
		}

	} else {

		status = uarpPlatformAccessoryAssetDeny(&accessory->accessory,
							&accessory->controller,
							asset);
		if (status != kUARPStatusSuccess) {
			LOG_ERR("uarpPlatformAccessoryAssetDeny failed, status 0x%04X", status);
		}
	}
}

static void dynamic_asset_offered(void *accessory_delegate, void *controller_delegate,
				  struct uarpPlatformAsset *asset)
{
	return;
}

static void remove_asset(struct fmna_uarp_accessory *accessory,
			 struct uarpPlatformAsset *asset,
			 bool release)
{
	int ret;

	if (asset != accessory->asset) {
		return;
	}

	if (release) {
		uarpPlatformAccessoryAssetRelease(&accessory->accessory, NULL, asset);
	}

	accessory->state = ASSET_NONE;
	accessory->asset = NULL;

	if (accessory->dfu_target_init_done) {
		ret = dfu_target_reset();
		if (ret != 0) {
			LOG_ERR("Cannot reset DFU target, code %d", ret);
		}
	}
}

static void asset_rescinded(void *accessory_delegate, void *controller_delegate,
			    void *asset_delegate)
{
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	if (controller_delegate) {
		LOG_INF("Asset %u Rescinded", asset->core.assetID);
	} else {
		LOG_INF("Asset %u Corrupt", asset->core.assetID);
	}

	remove_asset(accessory, asset, false);
}

static void asset_corrupt(void *accessory_delegate, void *asset_delegate)
{
	asset_rescinded(accessory_delegate, NULL, asset_delegate);
}

static void asset_orphaned(void *accessory_delegate, void *asset_delegate)
{
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *)accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *)asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	if (asset != accessory->asset) {
		return;
	}

	switch (accessory->state) {
	case ASSET_ACTIVE:
		accessory->state = ASSET_ORPHANED;
		break;

	case ASSET_STAGED:
		/* Resuming fully staged asset will cause assert in UARPDK, so it must be removed */
	case ASSET_FAILED:
		remove_asset(accessory, asset, true);
		break;

	default:
		break;
	}
}

static void report_failure(struct fmna_uarp_accessory *accessory,
			   struct uarpPlatformAsset *asset,
			   uint32_t last_error,
			   uint32_t last_error_info)
{
	uint32_t status;

	LOG_ERR("Fatal update failure, error %d, info %d", last_error, (int16_t)last_error_info);

	accessory->last_error = (last_error << 16) | (last_error_info & 0xFFFF);

	switch (accessory->state) {
	case ASSET_ACTIVE:
		accessory->state = ASSET_FAILED;
		status = uarpPlatformAccessoryAssetFullyStaged(&accessory->accessory, asset);
		if (status != kUARPStatusSuccess) {
			LOG_ERR("uarpPlatformAccessoryAssetFullyStaged failed, status 0x%04X",
				status);
		}
		break;

	case ASSET_STAGED:
		accessory->state = ASSET_FAILED;
		break;

	case ASSET_ORPHANED:
		remove_asset(accessory, asset, true);
		break;

	default:
		break;
	}
}

static void asset_ready(void *accessory_delegate, void *asset_delegate)
{
	uint32_t status;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	status = uarpPlatformAccessoryAssetRequestMetaData(&accessory->accessory, asset);

	if (status == kUARPStatusNoMetaData) {
		asset_meta_data_complete(accessory_delegate, asset_delegate);
	} else if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformAccessoryAssetRequestMetaData failed, status 0x%04X", status);
		report_failure(accessory, asset, LAST_ERROR_ASSET_REQUEST_METADATA_FAILED, status);
	}
}

static void asset_meta_data_tlv(void *accessory_delegate, void *asset_delegate,
				uint32_t type, uint32_t length, uint8_t *value)
{
	LOG_INF("SuperBinary MetaData type 0x%08X, length %d", type, length);
}

static void asset_meta_data_complete(void *accessory_delegate, void *asset_delegate)
{
	uint32_t status;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	status = uarpPlatformAssetSetPayloadIndex(&accessory->accessory, asset, 0);

	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformAssetSetPayloadIndex failed, status 0x%04X", status);
		report_failure(accessory, asset, LAST_ERROR_ASSET_SET_PAYLOAD_INDEX_FAILED, status);
	}
}

static void payload_ready(void *accessory_delegate, void *asset_delegate)
{
	static const char payload4cc[] = CONFIG_FMNA_UARP_PAYLOAD_4CC;
	
	uint32_t status;
	UARPVersionComparisonResult comparison_result;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	BUILD_ASSERT(sizeof(payload4cc) == PAYLOAD_4CC_LENGTH,
		     "Invalid payload 4CC length. Check FMNA_UARP_PAYLOAD_4CC configuration.");

	LOG_INF("Payload Ready - Index %d Tag <%c%c%c%c>",
		asset->selectedPayloadIndex,
		asset->payload.payload4cc[0],
		asset->payload.payload4cc[1],
		asset->payload.payload4cc[2],
		asset->payload.payload4cc[3]);

	LOG_INF("Payload Ready - Ver %d.%d.%d.%d Len %d",
		asset->payload.plHdr.payloadVersion.major,
		asset->payload.plHdr.payloadVersion.minor,
		asset->payload.plHdr.payloadVersion.release,
		asset->payload.plHdr.payloadVersion.build,
		asset->payload.plHdr.payloadLength);

	query_active_firmware_version(accessory_delegate, 0, &accessory->payload_version);

	comparison_result = uarpVersionCompare(&accessory->payload_version,
					       &asset->payload.plHdr.payloadVersion);

	if (comparison_result == kUARPVersionComparisonResultIsNewer &&
	    memcmp(payload4cc, asset->payload.payload4cc, sizeof(asset->payload.payload4cc)) == 0) {

		accessory->apply_flags = kUARPApplyStagedAssetsFlagsNeedsRestart;
		accessory->payload_version = asset->payload.plHdr.payloadVersion;
		ocrypto_sha256_init(&accessory->hash_ctx);
		memset(accessory->payload_hash, 0, sizeof(accessory->payload_hash));

		status = uarpPlatformAccessoryPayloadRequestMetaData(&accessory->accessory, asset);
		if (status == kUARPStatusNoMetaData) {
			payload_meta_data_complete(accessory_delegate, asset_delegate);
		} else if (status != kUARPStatusSuccess) {
			LOG_ERR("uarpPlatformAccessoryPayloadRequestMetaData failed, status 0x%04X",
				status);
			report_failure(accessory, asset, LAST_ERROR_PAYLOAD_REQUEST_METADATA_FAILED,
				       status);
		}

	} else if (asset->selectedPayloadIndex + 1 < asset->core.assetNumPayloads) {

		LOG_INF("Moving to payload %d of %d",
			asset->selectedPayloadIndex + 1,
			asset->core.assetNumPayloads );
		status = uarpPlatformAssetSetPayloadIndex(&accessory->accessory,
							  asset,
							  asset->selectedPayloadIndex + 1);
		if (status != kUARPStatusSuccess) {
			LOG_ERR("uarpPlatformAssetSetPayloadIndex failed, status 0x%04X", status);
			report_failure(accessory, asset, LAST_ERROR_ASSET_SET_PAYLOAD_INDEX_FAILED,
				       status);
		}

	} else {

		LOG_ERR("No applicable payload");
		report_failure(accessory, asset, LAST_ERROR_NO_APPLICABLE_PAYLOAD,
			       asset->core.assetNumPayloads);
	}
}

static void payload_meta_data_tlv(void *accessory_delegate,
				  void *asset_delegate,
				  uint32_t type,
				  uint32_t length,
				  uint8_t *value)
{
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");
	__ASSERT(value, "NULL parameter");
	__ASSERT(length < CONFIG_FMNA_UARP_PAYLOAD_WINDOW_SIZE, "Invalid length");

	LOG_INF("Payload MetaData type 0x%08X, length %d", type, length);

	switch (type)
	{
	case TLV_TYPE_SHA2:
		(void)accessory;
		if (length == ocrypto_sha256_BYTES) {
			memcpy(accessory->payload_hash, value, length);
		} else {
			LOG_ERR("Invalid hash length. Only SHA-256 is supported.");
			report_failure(accessory, asset, LAST_ERROR_INVALID_HASH_TLV_LENGTH,
				       length);
		}
		break;

	case TLV_TYPE_APPLY_FLAGS:
		if (length == 1) {
			accessory->apply_flags = value[0];
		} else {
			LOG_ERR("Invalid apply flags TLV");
			report_failure(accessory, asset, LAST_ERROR_INVALID_APPLY_FLAGS_TLV_LENGTH,
				       length);
		}
		break;
	
	default:
		break;
	}
}

static void dfu_target_callback(enum dfu_target_evt_id evt_id)
{
	/* Can be ignored for MCUBoot target. */
}

static void payload_meta_data_complete(void *accessory_delegate, void *asset_delegate)
{
	static uint8_t mcuboot_buf[CONFIG_FMNA_UARP_MCUBOOT_BUF_SIZE] __aligned(4);
	int ret;
	uint32_t status;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");

	if (accessory->state != ASSET_ACTIVE) {
		return;
	}

	if (accessory->dfu_target_init_done) {
		ret = dfu_target_reset();
		if (ret) {
			LOG_ERR("dfu_target_reset failed, code %d", ret);
			goto error_exit;
		}
	}

	ret = dfu_target_mcuboot_set_buf(mcuboot_buf, sizeof(mcuboot_buf));
	if (ret) {
		LOG_ERR("dfu_target_mcuboot_set_buf failed, code %d", ret);
		goto error_exit;
	}

	if (accessory->dfu_target_init_done) {
		ret = dfu_target_mcuboot_init(asset->payload.plHdr.payloadLength,
					      0,
					      dfu_target_callback);
		if (ret) {
			LOG_ERR("dfu_target_mcuboot_init failed, code %d", ret);
			goto error_exit;
		}
	} else {
		ret = dfu_target_init(DFU_TARGET_IMAGE_TYPE_MCUBOOT,
				      0,
				      asset->payload.plHdr.payloadLength,
				      dfu_target_callback);
		if (ret) {
			LOG_ERR("dfu_target_init failed, code %d", ret);
			goto error_exit;
		}

		accessory->dfu_target_init_done = true;
	}

	status = uarpPlatformAccessoryPayloadRequestData(&accessory->accessory, asset);
	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformAccessoryPayloadRequestData failed, status 0x%04X", status);
		report_failure(accessory, asset, LAST_ERROR_PAYLOAD_REQUEST_DATA_FAILED, status);
	}

	return;

error_exit:
	report_failure(accessory, asset, LAST_ERROR_DFU_TARGET_INIT_FAILED, ret);
}

static void payload_data(void *accessory_delegate, void *asset_delegate,
			 uint8_t *buffer, uint32_t buffer_length, uint32_t offset,
			 uint8_t *asset_state, uint32_t asset_state_length)
{
	int ret;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");
	__ASSERT(buffer, "NULL parameter");
	__ASSERT(buffer_length <= asset->payload.plHdr.payloadLength, "Invalid length");
	__ASSERT(offset <= asset->payload.plHdr.payloadLength, "Invalid offset");
	__ASSERT(offset + buffer_length <= asset->payload.plHdr.payloadLength, "Invalid length");

	ocrypto_sha256_update(&accessory->hash_ctx, buffer, buffer_length);

	ret = dfu_target_write(buffer, buffer_length);

	if (ret) {
		LOG_ERR("Image write error, code %d", ret);
		report_failure(accessory, asset, LAST_ERROR_IMAGE_WRITE_FAILED, ret);
	}
}

static void reboot_work_handler(struct k_work *work)
{
	LOG_INF("Rebooting caused by applied UARP update.");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_COLD);
}

static int apply_and_reboot(struct fmna_uarp_accessory *accessory,
			    struct uarpPlatformAsset *asset,
			    k_timeout_t delay)
{
	static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_handler);
	int ret;

	ret = dfu_target_done(true);
	if (ret == 0) {
		ret = dfu_target_schedule_update(0);
	}

	if (ret) {
		LOG_ERR("Apply Staged Assets: Applying failed");
		report_failure(accessory, asset, LAST_ERROR_DFU_DONE_FAILED, ret);
	} else {
		LOG_INF("Apply Staged Assets: Updating Active FW Version to Staged FW Version");
		k_work_reschedule(&reboot_work, delay);
		accessory->state = ASSET_APPLIED;
	}

	return ret;
}

static void payload_data_complete(void *accessory_delegate, void *asset_delegate)
{
	uint8_t hash[ocrypto_sha256_BYTES];
	uint32_t status;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	struct uarpPlatformAsset *asset = (struct uarpPlatformAsset *) asset_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(asset_delegate, "NULL parameter");
	__ASSERT(asset == accessory->asset, "Invalid parameter");

	if (accessory->state != ASSET_ACTIVE) {
		return;
	}

	ocrypto_sha256_final(&accessory->hash_ctx, hash);

	if (memcmp(hash, accessory->payload_hash, ocrypto_sha256_BYTES) != 0) {
		LOG_ERR("Invalid hash");
		report_failure(accessory, asset, LAST_ERROR_INVALID_HASH,
			       accessory->payload_hash[1] | (accessory->payload_hash[0] << 8));
	} else if (accessory->apply_flags == APPLY_FLAGS_FAST_RESET) {
		apply_and_reboot(accessory, accessory->asset, K_MSEC(1));
	} else {
		accessory->state = ASSET_STAGED;
		status = uarpPlatformAccessoryAssetFullyStaged(&accessory->accessory, asset);
		if (status != kUARPStatusSuccess) {
			LOG_ERR("uarpPlatformAccessoryAssetFullyStaged failed, status 0x%04X",
				status);
			report_failure(accessory, asset, LAST_ERROR_ASSET_FULLY_STAGED_FAILED,
				       status);
		}
	}
}

static uint32_t apply_staged_assets(void *accessory_delegate,
				    void *controller_delegate,
				    uint16_t *flags)
{
	int ret;
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;
	
	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(controller_delegate, "NULL parameter");
	__ASSERT(flags, "NULL parameter");

	switch (accessory->state) {
	case ASSET_NONE:
		LOG_ERR("Apply Staged Assets: Nothing staged");
		*flags = kUARPApplyStagedAssetsFlagsNothingStaged;
		break;

	case ASSET_FAILED:
	case ASSET_APPLIED:
	case ASSET_ORPHANED:
		LOG_ERR("Apply Staged Assets: Failure");
		*flags = kUARPApplyStagedAssetsFlagsFailure;
		break;

	case ASSET_ACTIVE:
		LOG_ERR("Apply Staged Assets: Staging SuperBinary");
		*flags = kUARPApplyStagedAssetsFlagsMidUpload;
		break;

	case ASSET_STAGED:
		ret = apply_and_reboot(accessory, accessory->asset,
				       K_MSEC(CONFIG_FMNA_UARP_REBOOT_DELAY_TIME));
		if (ret) {
			*flags = kUARPApplyStagedAssetsFlagsFailure;
		} else {
			*flags = accessory->apply_flags;
		}
		break;

	default:
		break;
	}

	return kUARPStatusSuccess;
}

static uint32_t query_string(const char* value, uint8_t *option_string, uint32_t *length)
{
	uint32_t length_needed;
	
	__ASSERT(value, "NULL parameter");
	__ASSERT(option_string, "NULL parameter");
	__ASSERT(length, "NULL parameter");

	length_needed = strlen(value);
	if (length_needed > *length) {
		LOG_ERR("Cannot fit string '%s' into TX message", value);
		return kUARPStatusInvalidLength;
	}

	*length = length_needed;
	memcpy(option_string, value, *length);
	
	return kUARPStatusSuccess;
}

static uint32_t query_manufacturer_name(void *accessory_delegate,
					uint8_t *option_string,
					uint32_t *length)
{
	return query_string(CONFIG_FMNA_MANUFACTURER_NAME, option_string, length);
}

static uint32_t query_model_name(void *accessory_delegate,
				 uint8_t *option_string,
				 uint32_t *length)
{
	return query_string(CONFIG_FMNA_MODEL_NAME, option_string, length);
}

static uint32_t query_serial_number(void *accessory_delegate,
				    uint8_t *option_string,
				    uint32_t *length)
{
	int err;
	char serial_number[FMNA_SERIAL_NUMBER_BLEN];

	err = fmna_serial_number_get(serial_number);
	if (err) {
		LOG_ERR("UARP Serial Number read failed");
		memset(serial_number, 0, sizeof(serial_number));
	}

	return query_string(serial_number, option_string, length);
}

static uint32_t query_hardware_version(void *accessory_delegate,
				       uint8_t *option_string,
				       uint32_t *length)
{
	return query_string(CONFIG_FMNA_HARDWARE_VERSION, option_string, length);
}

static uint32_t query_active_firmware_version(void *accessory_delegate,
					      uint32_t asset_tag,
					      struct UARPVersion *version)
{
	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(version, "NULL parameter");

	if (!asset_tag) {
		int err;
		struct fmna_version ver;

		err = fmna_version_fw_get(&ver);
		if (err) {
			LOG_ERR("UARP Firmware Version read failed");
			memset(&ver, 0, sizeof(ver));
		}

		version->major = ver.major;
		version->minor = ver.minor;
		version->release = ver.revision;
		version->build = ver.build_num;

		return kUARPStatusSuccess;
	} else {
		LOG_ERR("Invalid asset tag");
		memset(version, 0, sizeof(*version));
		return kUARPStatusInvalidAssetTag;
	}
}

static uint32_t query_staged_firmware_version(void *accessory_delegate,
					      uint32_t asset_tag,
					      struct UARPVersion *version)
{
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(version, "NULL parameter");

	memset(version, 0, sizeof(*version));
		
	if (asset_tag == 0) {
		if (accessory->state == ASSET_STAGED) {
			*version = accessory->payload_version;
		} else {
			LOG_WRN("No staged version to return");
		}
		return kUARPStatusSuccess;
	} else {
		LOG_ERR("Invalid asset tag");
		return kUARPStatusInvalidAssetTag;
	}
}

static uint32_t query_last_error(void *accessory_delegate, struct UARPLastErrorAction *last)
{
	struct fmna_uarp_accessory *accessory = (struct fmna_uarp_accessory *) accessory_delegate;

	__ASSERT(accessory_delegate, "NULL parameter");
	__ASSERT(last, "NULL parameter");

	last->lastAction = kUARPLastActionApplyFirmwareUpdate;
	last->lastError = 0;
	if (accessory->last_error == LAST_ERROR_UNSET) {
		/* TODO: detect errors on reboot
		 - last test image failed to confirm
		 - last image failed to update (e.g. invalid signature)
		 */
		last->lastError = LAST_ERROR_NONE;
	} else {
		last->lastError = accessory->last_error;
	}

	LOG_INF("Returned last error: %d, info %d",
		last->lastError >> 16,
		(int16_t) last->lastError);

	return kUARPStatusSuccess;
}

int fmna_uarp_img_confirm(void)
{
	int ret;

	ret = boot_write_img_confirmed();

	if (ret) {
		LOG_ERR("Cannot confirm current image, code %d", ret);
	} else {
		LOG_INF("Current image confirmed");
	}

	return ret;
}

bool fmna_uarp_init(fmna_uarp_send_message_fn send_message_callback)
{
	uint32_t status;
	struct uarpPlatformOptionsObj options;
	struct uarpPlatformAccessoryCallbacks callbacks;

	__ASSERT(send_message_callback, "NULL parameter");

	LOG_INF("Initializing FMNA UARP");

	options.maxTxPayloadLength = CONFIG_FMNA_UARP_TX_MSG_PAYLOAD_SIZE;
	options.maxRxPayloadLength = CONFIG_FMNA_UARP_RX_MSG_PAYLOAD_SIZE;
	options.payloadWindowLength = CONFIG_FMNA_UARP_PAYLOAD_WINDOW_SIZE;
	
	accessory.send_message = send_message_callback;
	
	callbacks.fRequestBuffer = request_buffer;
	callbacks.fReturnBuffer = return_buffer;
	callbacks.fRequestTransmitMsgBuffer = request_transmit_msg_buffer;
	callbacks.fReturnTransmitMsgBuffer = return_transmit_msg_buffer;
	callbacks.fSendMessage = send_message;
	callbacks.fDataTransferPause = data_transfer_pause;
	callbacks.fDataTransferResume = data_transfer_resume;
	callbacks.fSuperBinaryOffered = super_binary_offered;
	callbacks.fDynamicAssetOffered = dynamic_asset_offered;
	callbacks.fAssetOrphaned = asset_orphaned;
	callbacks.fAssetRescinded = asset_rescinded;
	callbacks.fAssetCorrupt = asset_corrupt;
	callbacks.fAssetReady = asset_ready;
	callbacks.fAssetMetaDataTLV = asset_meta_data_tlv;
	callbacks.fAssetMetaDataComplete = asset_meta_data_complete;
	callbacks.fPayloadReady = payload_ready;
	callbacks.fPayloadMetaDataTLV = payload_meta_data_tlv;
	callbacks.fPayloadMetaDataComplete = payload_meta_data_complete;
	callbacks.fPayloadData = payload_data;
	callbacks.fPayloadDataComplete = payload_data_complete;
	callbacks.fApplyStagedAssets = apply_staged_assets;
	callbacks.fManufacturerName = query_manufacturer_name;
	callbacks.fModelName = query_model_name;
	callbacks.fSerialNumber = query_serial_number;
	callbacks.fHardwareVersion = query_hardware_version;
	callbacks.fActiveFirmwareVersion = query_active_firmware_version;
	callbacks.fStagedFirmwareVersion = query_staged_firmware_version;
	callbacks.fLastError = query_last_error;
	
	status = uarpPlatformAccessoryInit(&accessory.accessory, &options, &callbacks, NULL, NULL,
					   (void *) &accessory);

	if (status != kUARPStatusSuccess) {
		LOG_ERR("uarpPlatformControllerAdd failed, status 0x%04X", status);
		return false;
	}
	
	return true;
}

static void owner_connected_cmd_handle(void)
{
	fmna_uarp_img_confirm();
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_fmna_event(aeh)) {
		struct fmna_event *event = cast_fmna_event(aeh);

		if (event->id == FMNA_EVENT_OWNER_CONNECTED) {
			owner_connected_cmd_handle();
		}
	}

	return false;
}

APP_EVENT_LISTENER(uarp_fmna_state, app_event_handler);
APP_EVENT_SUBSCRIBE(uarp_fmna_state, fmna_event);
