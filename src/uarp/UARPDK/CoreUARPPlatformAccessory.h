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

#ifndef CoreUARPPlatformAccessory_h
#define CoreUARPPlatformAccessory_h

#include "CoreUARPPlatform.h"
#include "CoreUARPUtils.h"
#include "CoreUARPAccessory.h"
#include "CoreUARPProtocolDefines.h"

struct uarpPlatformAsset;

/* Unfortunately, we had some typos and we don't want to break any backwards compatibility */
#define fcnUarpPlatformAAccessoryRequestBuffer fcnUarpPlatformAccessoryRequestBuffer
#define fcnUarpPlatformAAccessoryReturnBuffer fcnUarpPlatformAccessoryReturnBuffer


/* -------------------------------------------------------------------------------- */
/*! @brief request buffer for general usage
    @discussion this routine is called when the lower layer would like a dyamically allocated buffer
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param ppBuffer double pointer where the buffer pointer will be returned. Callback managed.  Caller MUST NOT free this memory
    @param length the length of the buffer needed
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (* fcnUarpPlatformAccessoryRequestBuffer)( void *pAccessoryDelegate, uint8_t **ppBuffer,
                                                           uint32_t length );


/* -------------------------------------------------------------------------------- */
/*! @brief return previously allocated buffer
    @discussion this routine is called when the lower layer is done with a dynamically allocated buffer
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pBuffer pointer to the buffer being returned
 */
/* -------------------------------------------------------------------------------- */
typedef void (* fcnUarpPlatformAccessoryReturnBuffer)( void *pAccessoryDelegate, uint8_t *pBuffer );


/* -------------------------------------------------------------------------------- */
/*! @brief pause data transfers
    @discussion this routine is called when the controller has requested to pause data transfers
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pControllerDelegate pointer to the firmware updater delegate's controller context pointer
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (* fcnUarpPlatformAccessoryDataTransferPause)( void *pAccessoryDelegate, void *pControllerDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief resume data transfers
    @discussion this routine is called when the controller has requested to resume data transfers
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pControllerDelegate pointer to the firmware updater delegate's controller context pointer
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (* fcnUarpPlatformAccessoryDataTransferResume)( void *pAccessoryDelegate, void *pControllerDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief Accessory specific implementation to handle an asset being offered
    @discussion This routine is called when the platform accessory layer has been offered a UARP asset from a remote controller
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pControllerDelegate pointer to the accessory specific layer's representation of a UARP controller
    @param pAsset pointer to the asset being offered
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryAssetOffered)( void *pAccessoryDelegate,
                                                     void *pControllerDelegate,
                                                     struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Accessory specific implementation to handle an asset is ready for transfer processing
    @discussion This routine is called asynchronously in response to the accessory specific layer accepting an offerred asset.
                After this has been called, the accessory specific implementation can begin to process the payload(s) [and any metadata].
                If this asset is a superbinary, the superbinary header will have been read.
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate  pointer to the accessory specific layer's representation of the asset which has been accepted
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryAssetReady)( void *pAccessoryDelegate,
                                                   void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief MetaData TLV has been received
    @discussion This routine is called to inform the accsessory specific layer of a TLV in the Asset's MetaData
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate pointer to the accessory specific layer's representation of an asset
    @param tlvType type field of the MetaData TLV
    @param tlvLength length field of the MetaData TLV
    @param pTlvValue pointer to the value of the MetaData TLV
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryMetaDataTLV)( void *pAccessoryDelegate,
                                                    void *pAssetDelegate,
                                                    uint32_t tlvType,
                                                    uint32_t tlvLength,
                                                    uint8_t *pTlvValue );

/* -------------------------------------------------------------------------------- */
/*! @brief MetaData has been completely processed
    @discussion This routine is called to inform the accsessory specific layer that the SuperBinary Asset's MetaData has been completely processed
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate pointer to the accessory specific layer's representation of an asset
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryMetaDataComplete)( void *pAccessoryDelegate,
                                                         void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief Accessory specific implementation to handle a selected payload is ready for transfer processing
    @discussion This routine is called asynchronously in response to the accessory specific layer accepting an offerred asset.
                After this has been called, the accessory specific implementation can begin to process the selected payload data [and any metadata].
                If this asset is a superbinary, the superbinary header will have been read.
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate  pointer to the accessory specific layer's representation of the asset which has been accepted
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryPayloadReady)( void *pAccessoryDelegate,
                                                     void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief Payload Data has been received
    @discussion This routine is called when the platform accessory layer has been offered a UARP asset from a remote controller
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate pointer to the accessory specific layer's representation of an asset
    @param pBuffer pointer to the payload window buffer
    @param lengthBuffer length of valid data in the payload window buffer
    @param offset offset into the payload being transferred, relative to the payload's beginning offset
    @param pAssetState opaque pointer to the firmware updater's representation of an asset; may be used to restore transfer state at power on;
                        actually points to <struct uarpFwUpAssetObj>
    @param lengthAssetState length of the pointer represented by the asset state opaque pointers
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryPayloadData)( void *pAccessoryDelegate,
                                                    void *pAssetDelegate,
                                                    uint8_t *pBuffer,
                                                    uint32_t lengthBuffer,
                                                    uint32_t offset,
                                                    uint8_t *pAssetState,
                                                    uint32_t lengthAssetState );

/* -------------------------------------------------------------------------------- */
/*! @brief Payload Data has been completely processed
    @discussion This routine is called to inform the accsessory specific layer that the selected payload's data has been completely processed
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pAssetDelegate pointer to the accessory specific layer's representation of an asset
 */
/* -------------------------------------------------------------------------------- */
typedef void (*fcnUarpPlatformAccessoryPayloadDataComplete)( void *pAccessoryDelegate,
                                                            void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief asset release callback
    @discussion this routine is called when the lower layer is ready to release and free all objects associated with an asset
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pAssetDelegate pointer to the firmware updater delegate's asset context pointer
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (* fcnUarpPlatformAccessoryAssetRelease)( void *pAccessoryDelegate, void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief asset orphan callback
    @discussion this routine is called when the lower layer has orphaned the asset; because the controller has become unreachable
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pAssetDelegate pointer to the firmware updater delegate's asset context pointer
 */
/* -------------------------------------------------------------------------------- */
typedef void (* fcnUarpPlatformAccessoryAssetOrphan)( void *pAccessoryDelegate, void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief controller has rescinded an asset
    @discussion this routine is called when the controller has rescinded a previously offered an asset to the accessory
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pControllerDelegate pointer to the firmware updater delegate's controller context pointer
    @param pAssetDelegate pointer to a firmware update delegate's asset context pointer
 */
/* -------------------------------------------------------------------------------- */
typedef void (* fcnUarpPlatformAccessoryAssetRescinded)( void *pAccessoryDelegate, void *pControllerDelegate,
                                                        void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief asset corrupt
    @discussion this routine is called when the stack has found that an asset is corrupt
    @param pAccessoryDelegate pointer to the firmware updater delegate's accessory context pointer
    @param pAssetDelegate pointer to a firmware update delegate's asset context pointer
 */
/* -------------------------------------------------------------------------------- */
typedef void (* fcnUarpPlatformAccessoryAssetCorrupt)( void *pAccessoryDelegate, void *pAssetDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief Query Info String
    @discussion This routine is called to query the accsessory specific layer for the accessory's info option
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pOptionString pointer to the memory location to copy the info option
    @param pLength pointer to return the length of the info option
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (*fcnUarpPlatformAccessoryQueryInfoString)( void *pAccessoryDelegate,
                                                            uint8_t *pOptionString,
                                                            uint32_t *pLength );

/* -------------------------------------------------------------------------------- */
/*! @brief Query Info Version
    @discussion This routine is called to query the accsessory specific layer for the accessory's info option
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param assetTag the 4cc of the asset in question
    @param pVersion double pointer to return the info version
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (*fcnUarpPlatformAccessoryQueryInfoVersion)( void *pAccessoryDelegate,
                                                             uint32_t assetTag,
                                                             struct UARPVersion *pVersion );

/* -------------------------------------------------------------------------------- */
/*! @brief Query Last Error
    @discussion This routine is called to query the accsessory specific layer for the accessory's info option
    @param pAccessoryDelegate pointer to the accessory specific layer's representation of a UARP accessory
    @param pLast pointer to return the last action with an error / status
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
typedef uint32_t (*fcnUarpPlatformAccessoryQueryLastError)( void *pAccessoryDelegate,
                                                           struct UARPLastErrorAction *pLast );

struct uarpPlatformAccessoryCallbacks
{
    fcnUarpPlatformAccessoryRequestBuffer fRequestBuffer;
    fcnUarpPlatformAccessoryReturnBuffer fReturnBuffer;
    fcnUarpAccessoryRequestTransmitMsgBuffer fRequestTransmitMsgBuffer;
    fcnUarpAccessoryReturnTransmitMsgBuffer fReturnTransmitMsgBuffer;
    fcnUarpAccessorySendMessage fSendMessage;
    fcnUarpPlatformAccessoryDataTransferPause fDataTransferPause;
    fcnUarpPlatformAccessoryDataTransferResume fDataTransferResume;
    fcnUarpPlatformAccessoryAssetOffered fSuperBinaryOffered;
    fcnUarpPlatformAccessoryAssetOffered fDynamicAssetOffered;
    fcnUarpPlatformAccessoryAssetRescinded fAssetRescinded;
    fcnUarpPlatformAccessoryAssetCorrupt fAssetCorrupt;
    fcnUarpPlatformAccessoryAssetOrphan fAssetOrphaned;
    fcnUarpPlatformAccessoryAssetReady fAssetReady;
    fcnUarpPlatformAccessoryMetaDataTLV fAssetMetaDataTLV;
    fcnUarpPlatformAccessoryMetaDataComplete fAssetMetaDataComplete;
    fcnUarpPlatformAccessoryPayloadReady fPayloadReady;
    fcnUarpPlatformAccessoryMetaDataTLV fPayloadMetaDataTLV;
    fcnUarpPlatformAccessoryMetaDataComplete fPayloadMetaDataComplete;
    fcnUarpPlatformAccessoryPayloadData fPayloadData;
    fcnUarpPlatformAccessoryPayloadDataComplete fPayloadDataComplete;
    fcnUarpAccessoryApplyStagedAssets fApplyStagedAssets;
    fcnUarpPlatformAccessoryQueryInfoString fManufacturerName;
    fcnUarpPlatformAccessoryQueryInfoString fModelName;
    fcnUarpPlatformAccessoryQueryInfoString fSerialNumber;
    fcnUarpPlatformAccessoryQueryInfoString fHardwareVersion;
    fcnUarpPlatformAccessoryQueryInfoVersion fActiveFirmwareVersion;
    fcnUarpPlatformAccessoryQueryInfoVersion fStagedFirmwareVersion;
    fcnUarpPlatformAccessoryQueryLastError fLastError;
};

struct uarpPlatformController
{
    struct uarpRemoteControllerObj _controller;

    void *pDelegate;

    struct uarpPlatformController *pNext;
};

struct uarpPlatformAccessory
{
    struct uarpAccessoryObj _accessory;

    struct uarpPlatformOptionsObj _options;

    void *pVendorExtension;

    struct uarpPlatformAccessoryCallbacks callbacks;
    
    void *pDelegate; /* passed into uarpPlatformAccessoryInit() */
    
    struct uarpPlatformAsset *pAssetList;
};

struct uarpPlatformAssetCookie
{
    uint32_t assetTag;
    struct UARPVersion assetVersion;
    uint32_t assetTotalLength;
    uint16_t assetNumPayloads;
    int selectedPayloadIndex;
    uint32_t lengthPayloadRecvd;
};

struct uarpPlatformAsset
{
    struct UARPSuperBinaryHeader sbHdr;

    struct uarpAssetCoreObj core;
    
    uint8_t internalFlags;
    
    /* only one outstanding data request can happen per asset */
    struct uarpDataRequestObj dataReq;

    UARPBool pausedByAccessory;

    /* only one payload is "active" at a time; even in a multiple payload superbinary */
    int selectedPayloadIndex;
    struct uarpPayloadObj payload;
    uint32_t lengthPayloadRecvd;

    /* the scratch buffer is used for pulling superbinary metadata, payload metadata and payload window chunks */
    uint8_t *pScratchBuffer;
    uint32_t lengthScratchBuffer;

    struct uarpPlatformController *pController;
    
    void *pDelegate;

    struct uarpPlatformAsset *pNext;
};

/* -------------------------------------------------------------------------------- */
/*! @brief Initialize a platform accessory
    @discussion This routine is called to initialize a platform accessory
    @param pAccessory pointer to the platform accessory, allocated by the caller
    @param pOptions pointer to accessory specific options, allocated by the caller
    @param pCallbacks pointer to a structure containing the
    @param pVendorExtension function pointer to vendor extension
    @param fVendorSpecific function pointer to handle vendor specific messages, allows for vendor specific implementations to be in a different file
    @param pDelegate pointer to the accessory specific layer's delegate for the
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryInit( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformOptionsObj *pOptions,
                                   struct uarpPlatformAccessoryCallbacks *pCallbacks,
                                   void *pVendorExtension,
                                   fcnUarpVendorSpecific fVendorSpecific,
                                   void *pDelegate );


/* -------------------------------------------------------------------------------- */
/*! @brief Add a controller to the accessory
    @discussion This routine is called to inform a platform accessory of reachable controller
    @param pAccessory pointer to the platform accessory, allocated by the caller
    @param pController pointer to the controller, allocated by the caller
    @param pControllerDelegate pointer to the accessory's representation of a remote controller
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformControllerAdd( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformController *pController,
                                   void *pControllerDelegate );

/* -------------------------------------------------------------------------------- */
/*! @brief Remove a controller to the accessory
    @discussion This routine is called to inform a platform accessory of a controller which is no longer reachable
    @param pAccessory pointer to the platform accessory, allocated by the caller
    @param pController pointer to the controller, allocated by the caller
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformControllerRemove( struct uarpPlatformAccessory *pAccessory,
                                      struct uarpPlatformController *pController );

/* -------------------------------------------------------------------------------- */
/*! @brief An acessory has received a UARP message  from the controller
    @discussion This routine is called to inform a platform accessory of a uarp message from controller
    @param pAccessory pointer to the platform accessory, allocated by the caller
    @param pController pointer to the platform controller's representation of the remote controller
    @param pBuffer pointer to the uarp message
    @param length length of the data pointed to the uarp message
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryRecvMessage( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformController *pController,
                                          uint8_t *pBuffer, uint32_t length );

/* -------------------------------------------------------------------------------- */
/*! @brief Is Asset Acceptable?
    @discussion This routine should be called by the accessory specifc layer in order to determine if we should deny this asset offer
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @param pIsAcceptable pointer to be returned
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetIsAcceptable( struct uarpPlatformAccessory *pAccessory,
                                                struct uarpPlatformAsset *pAsset,
                                                UARPBool *pIsAcceptable );

/* -------------------------------------------------------------------------------- */
/*! @brief Is Asset Cookie Acceptable?
    @discussion This routine should be called by the accessory specifc layer in order to determine if we should deny this asset offer
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @param pIsAcceptable pointer to be returned
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetCookieIsAcceptable( struct uarpPlatformAccessory *pAccessory,
                                                      struct uarpPlatformAsset *pAsset,
                                                      struct uarpPlatformAssetCookie *pCookie,
                                                      UARPBool *pIsAcceptable );

/* -------------------------------------------------------------------------------- */
/*! @brief Accept Asset
    @discussion This routine should be called by the accessory specifc layer when it wants to accept an offerred asset and start it's transfer.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pController pointer to the platform controller's representation of the remote controller
    @param pAsset pointer to the asset being accepted
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetAccept( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformController *pController,
                                          struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Accept Deny
    @discussion This routine should be called by the accessory specifc layer when it wants to deny an offerred asset
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pController pointer to the platform controller's representation of the remote controller
    @param pAsset pointer to the asset having it's active payload set
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetDeny( struct uarpPlatformAccessory *pAccessory,
                                        struct uarpPlatformController *pController,
                                        struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Accept Abandon
    @discussion This routine should be called by the accessory specifc layer when it wants to abandon a previously accepted asset
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pController pointer to the platform controller's representation of the remote controller
    @param pAsset pointer to the asset having it's active payload set
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetAbandon( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformController *pController,
                                           struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Accept Release
    @discussion This routine should be called by the accessory specifc layer when it is completely done processing the asset.
                This marks an asset as "ready for cleanup".  uarpPlatformCleanupAssets() should be called to free any buffers / allocated memory
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pController pointer to the platform controller's representation of the remote controller
    @param pAsset pointer to the asset having it's active payload set
    @return kUARPStatusXXX
*/
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetRelease( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformController *pController,
                                           struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Request MetaData for the Asset
    @discussion This routine should be called by the accessory specifc layer when it wants to request the asset's metadata
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetRequestMetaData( struct uarpPlatformAccessory *pAccessory,
                                                   struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Set Asset Payload Index
    @discussion This routine should be called by the accessory specifc layer to when it wants to change the active payload.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @param payloadIdx zero-based index to the payload to be operated on
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAssetSetPayloadIndex( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformAsset *pAsset,
                                          int payloadIdx );

/* -------------------------------------------------------------------------------- */
/*! @brief Set Asset Payload Index
    @discussion This routine should be called by the accessory specifc layer to when it wants to change the active payload.
                This variant is for an accessory who maintains asset transfer state across power loss.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @param payloadIdx zero-based index to the payload to be operated on
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAssetSetPayloadIndexWithCookie( struct uarpPlatformAccessory *pAccessory,
                                                    struct uarpPlatformAsset *pAsset,
                                                    int payloadIdx,
                                                    struct uarpPlatformAssetCookie *pCookie );

/* -------------------------------------------------------------------------------- */
/*! @brief Request MetaData for the payload
    @discussion This routine should be called by the accessory specifc layer when it wants to request the selected payload's metadata
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having metadata requested
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryPayloadRequestMetaData( struct uarpPlatformAccessory *pAccessory,
                                                     struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Set Asset Payload Offset
    @discussion This routine should be called by the accessory specifc layer to when it wants to change the offset into the active payload.
                    A good use of this when a power interruption occurred during an asset transfer.  The UARP Stack's merge routine would be oblivious.
                    This should be called between "metadata complete" and "request payload data"
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having it's active payload set
    @param payloadOffset  relative offset into the payload
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAssetSetPayloadOffset( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformAsset *pAsset,
                                           uint32_t payloadOffset );

/* -------------------------------------------------------------------------------- */
/*! @brief Request Data for the payload
    @discussion This routine should be called by the accessory specifc layer when it wants to request the selected payload's data.
                This will continue until the payload is completely transferred.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having payload data requested
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryPayloadRequestData( struct uarpPlatformAccessory *pAccessory,
                                                 struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Request Data for the payload
    @discussion This routine should be called by the accessory specifc layer when it wants to request the selected payload's data.
                This will continue until the payload is completely transferred.
                This variant is for an accessory who maintains asset transfer state across power loss.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset having payload data requested
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryPayloadRequestDataWithCookie( struct uarpPlatformAccessory *pAccessory,
                                                           struct uarpPlatformAsset *pAsset,
                                                           struct uarpPlatformAssetCookie *pCookie );

/* -------------------------------------------------------------------------------- */
/*! @brief Pause requesting data for the payload
    @discussion This routine should be called by the accessory specifc layer when it wants to pause trasnfer of the selected payload's data.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset whose data requests are being paused
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryPayloadRequestDataPause( struct uarpPlatformAccessory *pAccessory,
                                                      struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Resume requesting data for the payload
    @discussion This routine should be called by the accessory specifc layer when it wants to resume trasnfer of the selected payload's data.
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset whose data requests can resume
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryPayloadRequestDataResume( struct uarpPlatformAccessory *pAccessory,
                                                       struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Asset has been fully staged
    @discussion This routine should be called by the accessory specifc layer when it has fully staged an asset
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAsset pointer to the asset whose data requests can resume
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessoryAssetFullyStaged( struct uarpPlatformAccessory *pAccessory,
                                               struct uarpPlatformAsset *pAsset );

/* -------------------------------------------------------------------------------- */
/*! @brief Merge two superbinary assets
    @discussion Merge an equal offered superbinary into an abandoned superbinary; offered asset will be free'd
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pAssetOrphaned pointer to the orphaned asset
    @param pAssetOffered pointer to the offerred asset
    @return kUARPStatusXXX
 */
/* -------------------------------------------------------------------------------- */
uint32_t uarpPlatformAccessorySuperBinaryMerge( struct uarpPlatformAccessory *pAccessory,
                                               struct uarpPlatformAsset *pAssetOrphaned,
                                               struct uarpPlatformAsset *pAssetOffered );

/* -------------------------------------------------------------------------------- */
/*! @brief Send Message Complete
    @discussion Called by the accessory specific layer to inform this layer that the uarp message has been sent
    @param pAccessory pointer to the platfform accessory's representation of an accesory
    @param pController pointer to the platform controller's representation of the remote controller
    @param pBuffer pointer to the buffer representing the UARP message
 */
/* -------------------------------------------------------------------------------- */
void uarpPlatformAccessorySendMessageComplete( struct uarpPlatformAccessory *pAccessory,
                                              struct uarpPlatformController *pController,
                                              uint8_t *pBuffer );

/* -------------------------------------------------------------------------------- */
/*! @brief Cleanup assets
    @discussion Called by the accessory specific layer to do garabgae collection on assets marked for cleanup
    @param pAccessory pointer to the platfform accessory's representation of an accesory
 */
/* -------------------------------------------------------------------------------- */
void uarpPlatformCleanupAssets( struct uarpPlatformAccessory *pAccessory );

#endif /* CoreUARPPlatformAccessory_h */
