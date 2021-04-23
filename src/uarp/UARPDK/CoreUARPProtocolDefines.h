/*
 * Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
 * capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
 * Apple software is governed by and subject to the terms and conditions of your MFi License,
 * including, but not limited to, the restrictions specified in the provision entitled "Public
 * Software", and is further subject to your agreement to the following additional terms, and your
 * agreement that the use, installation, modification or redistribution of this Apple software
 * constitutes acceptance of these additional terms. If you do not agree with these additional terms,
 * you may not use, install, modify or redistribute this Apple software.
 *
 * Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
 * you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
 * license, under Apple's copyrights in this Apple software (the "Apple Software"), to use,
 * reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
 * redistribute the Apple Software, with or without modifications, in binary form, in each of the
 * foregoing cases to the extent necessary to develop and/or manufacture "Proposed Products" and
 * "Licensed Products" in accordance with the terms of your MFi License. While you may not
 * redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
 * form, you must retain this notice and the following text and disclaimers in all such redistributions
 * of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
 * used to endorse or promote products derived from the Apple Software without specific prior written
 * permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
 * express or implied, are granted by Apple herein, including but not limited to any patent rights that
 * may be infringed by your derivative works or by other works in which the Apple Software may be
 * incorporated. Apple may terminate this license to the Apple Software by removing it from the list
 * of Licensed Technology in the MFi License, or otherwise in accordance with the terms of such MFi License.
 *
 * Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
 * fixes or enhancements to Apple in connection with this software ("Feedback"), you hereby grant to
 * Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
 * reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
 * distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
 * and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
 * acknowledge and agree that Apple may exercise the license granted above without the payment of
 * royalties or further consideration to Participant.
 * The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
 * IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
 * IN COMBINATION WITH YOUR PRODUCTS.
 *
 * IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
 * AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2020 Apple Inc. All Rights Reserved.
 */


#ifndef uarpProtocolDefines_h
#define uarpProtocolDefines_h

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: If your compile supports packing structures differently,
    you will need to modify UARP_ATTRIBUTE_[POST][PRE]PACKED */
#define UARP_ATTRIBUTE_POST_PACKED   __attribute__((packed))

#define UARP_ATTRIBUTE_PRE_PACKED

#define kUARPProtocolVersionMinimum     1
#define kUARPProtocolVersion            1

#define kUARPMaxBytesRequested          0xFFFF

/* UARP Message Set Definition */

#define kUARPMsgSync                                        0x0000 /* A -> C, C -> A */
#define kUARPMsgVersionDiscoveryRequest                     0x0001 /* C -> A */
#define kUARPMsgVersionDiscoveryResponse                    0x0002 /* A -> C */
#define kUARPMsgAccessoryInformationRequest                 0x0003 /* C -> A */
#define kUARPMsgAccessoryInformationResponse                0x0004 /* A -> C */
#define kUARPMsgAssetAvailableNotification                  0x0005 /* C -> A --- */
#define kUARPMsgAssetDataRequest                            0x0006 /* A -> C --- */
#define kUARPMsgAssetDataResponse                           0x0007 /* C -> A --- */
#define kUARPMsgAssetDataTransferNotification               0x0008 /* C -> A */
#define kUARPMsgAssetProcessingNotification                 0x0009 /* A -> C */
#define kUARPMsgApplyStagedAssetsRequest                    0x000A /* C -> A */
#define kUARPMsgApplyStagedAssetsResponse                   0x000B /* A -> C */
#define kUARPMsgAssetRescindedNotification                  0x000C /* C -> A --- */
#define kUARPMsgAssetAvailableNotificationAck               0x000D /* A -> C --- */
#define kUARPMsgAssetDataTransferNotificationAck            0x000E /* A -> C --- */
#define kUARPMsgAssetProcessingNotificationAck              0x000F /* C -> A --- */
#define kUARPMsgAssetRescindedNotificationAck               0x0010

#if 0 // TODO: implement
#define kUARPMsgDebugModeRequest
#define kUARPMsgDebugModeResponse
#define kUARPMsgDebugNotification
#endif

#define kUARPMsgVendorSpecific                              0xFFFF

/* UARP Message Status Defintions */

#define kUARPStatusSuccess                                  0x0000
#define kUARPStatusUnknownMessageType                       0x0001
#define kUARPStatusIncompatibleProtocolVersion              0x0002
#define kUARPStatusInvalidAssetTag                          0x0003
#define kUARPStatusInvalidDataRequestOffset                 0x0004
#define kUARPStatusInvalidDataRequestLength                 0x0005
#define kUARPStatusMismatchUUID                             0x0006
#define kUARPStatusAssetDataPaused                          0x0007
#define kUARPStatusInvalidMessageLength                     0x0008
#define kUARPStatusInvalidMessage                           0x0009
#define kUARPStatusInvalidTLV                               0x000A
#define kUARPStatusNoResources                              0x000B
#define kUARPStatusUnknownAccessory                         0x000C
#define kUARPStatusUnknownController                        0x000D
#define kUARPStatusInvalidFunctionPointer                   0x000E
#define kUARPStatusCorruptSuperBinary                       0x000F
#define kUARPStatusAssetInFlight                            0x0010
#define kUARPStatusAssetIdUnknown                           0x0011
#define kUARPStatusInvalidDataResponseLength                0x0012
#define kUARPStatusAssetTransferComplete                    0x0013
#define kUARPStatusUnknownPersonalizationOption             0x0014
#define kUARPStatusMismatchPersonalizationOption            0x0015
#define kUARPStatusInvalidAssetType                         0x0016
#define kUARPStatusUnknownAsset                             0x0017
#define kUARPStatusNoVersionAgreement                       0x0018
#define kUARPStatusUnknownDataTransferState                 0x0019
#define kUARPStatusUnsupported                              0x001A
#define kUARPStatusInvalidObject                            0x001B
#define kUARPStatusUnknownInformationOption                 0x001C
#define kUARPStatusInvalidDataResponse                      0x001D
#define kUARPStatusInvalidArgument                          0x001E
#define kUARPStatusDataTransferPaused                       0x001F
#define kUARPStatusUnknownPayload                           0x0020
#define kUARPStatusInvalidDataTransferNotification          0x0021
#define kUARPStatusMetaDataTypeNotFound                     0x0022
#define kUARPStatusMetaDataCorrupt                          0x0023
#define kUARPStatusOutOfOrderMessageID                      0x0024
#define kUARPStatusDuplicateMessageID                       0x0025
#define kUARPStatusMismatchDataOffset                       0x0026
#define kUARPStatusInvalidLength                            0x0027
#define kUARPStatusNoMetaData                               0x0028
#define kUARPStatusDuplicateController                      0x0029
#define kUARPStatusDuplicateAccessory                       0x002A
#define kUARPStatusInvalidOffset                            0x002B
#define kUARPStatusInvalidPayload                           0x002C
#define kUARPStatusProcessingIncomplete                     0x002D
#define kUARPStatusInvalidDataRequestType                   0x002E
#define kUARPStatusInvalidSuperBinaryHeader                 0x002F
#define kUARPStatusInvalidPayloadHeader                     0x0030
#define kUARPStatusAssetNoBytesRemaining                    0x0031

/* we had some typos and do not want to break anyone's implementation */
#define kUARPStatusUnkownPersonalizationOption kUARPStatusUnknownPersonalizationOption


/* BOOL */

typedef uint8_t UARPBool;

#define kUARPNo     0
#define kUARPYes    1


/* Message Defintions */

#define kUARPInitialTxMsgID         1

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgHeader
{
    uint16_t msgType;
    uint16_t msgPayloadLength;
    uint16_t msgID;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPTLVHeader
{
    uint32_t tlvType;
    uint32_t tlvLength;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPVersion
{
    uint32_t major;
    uint32_t minor;
    uint32_t release;
    uint32_t build;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgVersionDiscoveryRequest
{
    struct UARPMsgHeader msgHdr;
    uint16_t protocolVersionController;
} UARP_ATTRIBUTE_POST_PACKED;

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgVersionDiscoveryResponse
{
    struct UARPMsgHeader msgHdr;
    uint16_t status;
    uint16_t protocolVersionAccessory;
} UARP_ATTRIBUTE_POST_PACKED;

#define kUARPTLVAccessoryInformationManufacturerName        0x00000001
#define kUARPTLVAccessoryInformationModelName               0x00000002
#define kUARPTLVAccessoryInformationSerialNumber            0x00000003
#define kUARPTLVAccessoryInformationHardwareVersion         0x00000004
#define kUARPTLVAccessoryInformationFirmwareVersion         0x00000005
#define kUARPTLVAccessoryInformationStagedFirmwareVersion   0x00000006
#define kUARPTLVAccessoryInformationStatistics              0x00000007
#define kUARPTLVAccessoryInformationLastError               0x00000008

/* TODO: Add support later */
#if 0
#define kUARPTLVAccessoryInformationAssetInfo
#define kUARPTLVAccessoryInformationCategory
#define kUARPTLVAccessoryInformationDebugParam
#endif

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAccessoryInformationRequest
{
    struct UARPMsgHeader msgHdr;
    uint32_t informationOption;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAccessoryInformationResponse
{
    struct UARPMsgHeader msgHdr;
    uint16_t status;
} UARP_ATTRIBUTE_POST_PACKED;

UARP_ATTRIBUTE_PRE_PACKED struct UARPStatistics
{
    uint32_t packetsNoVersionAgreement;
    uint32_t packetsMissed;
    uint32_t packetsDuplicate;
    uint32_t packetsOutOfOrder;
} UARP_ATTRIBUTE_POST_PACKED;

#define kUARPLastActionApplyFirmwareUpdate              0x00000001

UARP_ATTRIBUTE_PRE_PACKED struct UARPLastErrorAction
{
    uint32_t lastAction;
    uint32_t lastError;     /* Vendor Specific */
} UARP_ATTRIBUTE_POST_PACKED;


#define kUARPAssetFlagsAssetTypeSuperBinary         0x0001
#define kUARPAssetFlagsAssetTypeDynamic             0x0002

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetAvailableNotification
{
    struct UARPMsgHeader msgHdr;
    uint32_t assetTag;
    uint16_t assetFlags;
    uint16_t assetID;
    struct UARPVersion assetVersion;
    uint32_t assetLength;
    uint16_t assetNumPayloads;
} UARP_ATTRIBUTE_POST_PACKED;

#define kUARPAssetIDInvalid                         0x0000
#define kUARPAssetIDAllAssets                       0xFFFF

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetRescindedNotification
{
    struct UARPMsgHeader msgHdr;
    uint16_t assetID;
} UARP_ATTRIBUTE_POST_PACKED;

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetDataRequest
{
    struct UARPMsgHeader msgHdr;
    uint16_t assetID;
    uint32_t dataOffset;
    uint16_t numBytesRequested;
} UARP_ATTRIBUTE_POST_PACKED;

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetDataResponse
{
    struct UARPMsgHeader msgHdr;
    uint16_t status;
    uint16_t assetID;
    uint32_t dataOffset;
    uint16_t numBytesRequested;
    uint16_t numBytesResponded;
} UARP_ATTRIBUTE_POST_PACKED;


#define kUARPAssetDataTransferPause         0x0001
#define kUARPAssetDataTransferResume        0x0002

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetDataTransferNotification
{
    struct UARPMsgHeader msgHdr;
    uint16_t assetTransferFlags;
} UARP_ATTRIBUTE_POST_PACKED;


#define kUARPAssetProcessingFlagsUploadComplete     0x0001
#define kUARPAssetProcessingFlagsUploadDenied       0x0002
#define kUARPAssetProcessingFlagsUploadAbandoned    0x0003
#define kUARPAssetProcessingFlagsAssetCorrupt       0x0004

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgAssetProcessingNotification
{
    struct UARPMsgHeader msgHdr;
    uint16_t assetID;
    uint16_t assetProcessingFlags;
} UARP_ATTRIBUTE_POST_PACKED;


#define kUARPApplyStagedAssetsFlagsSuccess          0x0001
#define kUARPApplyStagedAssetsFlagsFailure          0x0002
#define kUARPApplyStagedAssetsFlagsNeedsRestart     0x0003
#define kUARPApplyStagedAssetsFlagsNothingStaged    0x0004
#define kUARPApplyStagedAssetsFlagsMidUpload        0x0005
#define kUARPApplyStagedAssetsFlagsInUse            0x0006

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgApplyStagedAssetsRequest
{
    struct UARPMsgHeader msgHdr;
} UARP_ATTRIBUTE_POST_PACKED;

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgApplyStagedAssetsResponse
{
    struct UARPMsgHeader msgHdr;
    uint16_t status;
    uint16_t flags;
} UARP_ATTRIBUTE_POST_PACKED;


#if 0 // MARK: needs implementation for kUARPMsgDebugModeRequest / kUARPMsgDebugModeResponse / kUARPMsgDebugNotification

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgDebugModeRequest
{
    struct UARPMsgHeader msgHdr;
    uint32_t mode;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgDebugModeResponse
{
    struct UARPMsgHeader msgHdr;
    uint16_t status;
    uint32_t mode;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgDebugNotification
{
    struct UARPMsgHeader msgHdr;
    uint32_t index;
} UARP_ATTRIBUTE_POST_PACKED;

#endif


/* OUI maintained here http://standards-oui.ieee.org/oui.txt */
#define kUARPOUILength    3

UARP_ATTRIBUTE_PRE_PACKED struct UARPMsgVendorSpecific
{
    struct UARPMsgHeader msgHdr;
    uint8_t oui[kUARPOUILength];
    uint16_t msgType;
} UARP_ATTRIBUTE_POST_PACKED;


#if 0 // MARK: for use with kUARPTLVAccessoryInformationAssetInfo

UARP_ATTRIBUTE_PRE_PACKED struct UARPTLVAssetInformation
{
    uint32_t type;
    uint32_t length;
    uint32_t assetTag;
    uint32_t runningVersion;
    uint32_t stagedVersion;
    uint32_t flags;
    /* description immediately follows, if length allows */
} UARP_ATTRIBUTE_POST_PACKED;
#endif

/* MARK: SUPERBINARY */

#define kUARPSuperBinaryFormatVersion               2

#define kUARPSuperBinaryPayloadTagLength            4

UARP_ATTRIBUTE_PRE_PACKED struct UARPSuperBinaryHeader
{
    uint32_t superBinaryFormatVersion;
    uint32_t superBinaryHeaderLength;
    uint32_t superBinaryLength;
    struct UARPVersion superBinaryVersion;
    uint32_t superBinaryMetadataOffset;
    uint32_t superBinaryMetadataLength;
    uint32_t payloadHeadersOffset;
    uint32_t payloadHeadersLength;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPPayloadHeader
{
    uint32_t payloadHeaderLength;
    uint32_t payloadTag;
    struct UARPVersion payloadVersion;
    uint32_t payloadMetadataOffset;
    uint32_t payloadMetadataLength;
    uint32_t payloadOffset;
    uint32_t payloadLength;
} UARP_ATTRIBUTE_POST_PACKED;


UARP_ATTRIBUTE_PRE_PACKED struct UARPSuperBinaryTLVHeader
{
    uint32_t tlvType;
    uint32_t tlvLength;
} UARP_ATTRIBUTE_POST_PACKED;

/* this is done to make "largest message header" calculation easy */

union UARPMessages
{
    struct UARPMsgVersionDiscoveryRequest discoveryRequest;
    struct UARPMsgVersionDiscoveryResponse discoveryResponse;
    struct UARPMsgAccessoryInformationRequest infoRequest;
    struct UARPMsgAccessoryInformationResponse infoResponse;
    struct UARPMsgAssetAvailableNotification assetAvailable;
    struct UARPMsgAssetRescindedNotification assetRescinded;
    struct UARPMsgAssetDataRequest dataRequest;
    struct UARPMsgAssetDataResponse dataResponse;
    struct UARPMsgAssetDataTransferNotification transferNotification;
    struct UARPMsgAssetProcessingNotification processingNotification;
    struct UARPMsgApplyStagedAssetsRequest applyAssetRequest;
    struct UARPMsgApplyStagedAssetsResponse applyAssetResponse;
    struct UARPMsgVendorSpecific vendorSpecific;
};

#ifdef __cplusplus
}
#endif

#endif /* uarpProtocolDefines_h */
