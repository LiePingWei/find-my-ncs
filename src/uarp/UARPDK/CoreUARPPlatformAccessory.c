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

#include "CoreUARPPlatformAccessory.h"

/* MARK: INTERNAL DEFINES */

#define kUARPDataRequestTypeInvalid                         0x00
#define kUARPDataRequestTypeSuperBinaryHeader               0x01
#define kUARPDataRequestTypeSuperBinaryPayloadHeader        0x02
#define kUARPDataRequestTypeSuperBinaryMetadata             0x04
#define kUARPDataRequestTypePayloadMetadata                 0x10
#define kUARPDataRequestTypePayloadPayload                  0x20
#define kUARPDataRequestTypeOutstanding                     0x80

#define kUARPPayloadIndexInvalid                            -1

#define kUARPAssetHasHeader                         0x01
#define kUARPAssetHasPayloadHeader                  0x02
#define kUARPAssetNeedsMetadata                     0x04
#define kUARPAssetHasMetadata                       0x08
#define kUARPAssetHasPayload                        0x10
#define kUARPAssetMarkForCleanup                    0x80

typedef uint32_t (* fcnUarpPlatformAssetDataRequestComplete)( struct uarpPlatformAccessory *pAccessory,
                                                             struct uarpPlatformAsset *pAsset,
                                                             uint8_t reqType, uint32_t payloadTag,
                                                             uint32_t offset, uint8_t * pBuffer, uint32_t length );

/* MARK: INTERNAL PROTOTYPES */

static struct uarpPlatformAsset *
    uarpPlatformAssetFindByAssetID( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformController *pController,
                                   uint16_t assetID );

static uint32_t uarpPlatformUpdateSuperBinaryMetaData( struct uarpPlatformAccessory *pAccessory,
                                                      struct uarpPlatformAsset *pAsset,
                                                      void *pBuffer, uint32_t lengthBuffer );

static uint32_t uarpPlatformUpdatePayloadMetaData( struct uarpPlatformAccessory *pAccessory,
                                                  struct uarpPlatformAsset *pAsset,
                                                  void *pBuffer, uint32_t lengthBuffer );

static uint32_t uarpPlatformUpdatePayloadPayload( struct uarpPlatformAccessory *pAccessory,
                                                 struct uarpPlatformAsset *pAsset,
                                                 void *pBuffer, uint32_t lengthBuffer );

static uint32_t uarpPlatformUpdateMetaData( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformAsset *pAsset,
                                           void *pBuffer, uint32_t lengthBuffer,
                                           fcnUarpPlatformAccessoryMetaDataTLV fMetaDataTLV,
                                           fcnUarpPlatformAccessoryMetaDataComplete fMetaDataComplete );

static void uarpPlatformCleanupAssetsForController( struct uarpPlatformAccessory *pAccessory,
                                                   struct uarpPlatformController *pController );

static uint32_t uarpPlatformAccessoryAssetSuperBinaryPullHeader( struct uarpPlatformAccessory *pAccessory,
                                                                struct uarpPlatformAsset *pAsset );

static UARPBool uarpPlatformAccessoryShouldRequestMetadata( uint8_t flags );

static uint32_t uarpPlatformAccessoryAssetAbandonInternal( struct uarpPlatformAccessory *pAccessory,
                                                          struct uarpPlatformController *pController,
                                                          struct uarpPlatformAsset *pAsset,
                                                          UARPBool notifyController );

static uint32_t uarpPlatformDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                                struct uarpPlatformAsset *pAsset,
                                                uint8_t reqType, uint32_t payloadTag,
                                                uint32_t offset, uint8_t * pBuffer, uint32_t length );

static uint32_t uarpPlatformSuperBinaryHeaderDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                                                 struct uarpPlatformAsset *pAsset,
                                                                 uint8_t reqType, uint32_t payloadTag,
                                                                 uint32_t offset, uint8_t * pBuffer, uint32_t length );

static uint32_t uarpPlatformAssetPayloadHeaderDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                                                  struct uarpPlatformAsset *pAsset,
                                                                  uint8_t reqType, uint32_t payloadTag,
                                                                  uint32_t offset, uint8_t * pBuffer, uint32_t length );

static uint32_t uarpPlatformAssetRequestDataContinue( struct uarpPlatformAccessory *pAccessory,
                                                     struct uarpPlatformController *pController,
                                                     struct uarpPlatformAsset *pAsset );

static uint32_t uarpPlatformAssetRequestData( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset,
                                             uint8_t requestType, uint32_t relativeOffset, uint32_t lengthNeeded );

static void uarpPlatformAssetCleanup( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset );

static void uarpPlatformAssetRelease( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset );

static void uarpPlatformAssetOrphan( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset );

static void uarpPlatformAssetRescind( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformController *pController,
                                     struct uarpPlatformAsset *pAsset );

static UARPBool uarpPlatformAssetIsCookieValid( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset,
                                               struct uarpPlatformAssetCookie *pCookie );

/* MARK: CALLBACK PROTOTYPES */

static uint32_t uarpPlatformRequestTransmitMsgBuffer( void *pAccessoryDelegate, void *pControllerDelegate,
                                                     uint8_t **ppBuffer, uint32_t *pLength );

static void uarpPlatformReturnTransmitMsgBuffer( void *pAccessoryDelegate, void *pControllerDelegate, uint8_t *pBuffer );

static uint32_t uarpPlatformSendMessage( void *pAccessoryDelegate, void *pControllerDelegate,
                                        uint8_t *pBuffer, uint32_t length );

static uint32_t uarpPlatformDataTransferPause( void *pAccessoryDelegate, void *pControllerDelegate );

static uint32_t uarpPlatformDataTransferResume( void *pAccessoryDelegate, void *pControllerDelegate );

static uint32_t uarpPlatformQueryAccessoryInfo( void *pAccessoryDelegate, uint32_t infoType, void *pBuffer,
                                               uint32_t lengthBuffer, uint32_t *pLengthNeeded );

static uint32_t uarpPlatformApplyStagedAssets( void *pAccessoryDelegate, void *pControllerDelegate, uint16_t *pFlags );

static void uarpPlatformAssetRescinded( void *pAccessoryDelegate, void *pControllerDelegate,
                                       uint16_t assetID );

static uint32_t uarpPlatformAssetDataResponse( void *pAccessoryDelegate, void *pControllerDelegate, uint16_t assetID,
                                              uint8_t *pBuffer, uint32_t length, uint32_t offset );

static uint32_t uarpPlatformAssetOffered( void *pAccessoryDelegate, void *pControllerDelegate,
                                         struct uarpAssetCoreObj *pAssetCore );

/* MARK: CONTROL ROUTINES */

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryInit( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformOptionsObj *pOptions,
                                   struct uarpPlatformAccessoryCallbacks *pCallbacks,
                                   void *pVendorExtension,
                                   fcnUarpVendorSpecific fVendorSpecific,
                                   void *pDelegate )
{
    uint32_t status;
    struct uarpAccessoryCallbacksObj callbacks;
    
    /* verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pOptions, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pCallbacks, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pDelegate, exit, status = kUARPStatusInvalidArgument );

    __UARP_Verify_Action( pCallbacks->fRequestBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fReturnBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fRequestTransmitMsgBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fReturnTransmitMsgBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fSendMessage, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fDataTransferPause, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fDataTransferResume, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fSuperBinaryOffered, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fDynamicAssetOffered, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetRescinded, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetCorrupt, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetOrphaned, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetReady, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetMetaDataTLV, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fAssetMetaDataComplete, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fPayloadReady, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fPayloadMetaDataTLV, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fPayloadMetaDataComplete, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fPayloadData, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fPayloadDataComplete, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fApplyStagedAssets, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fManufacturerName, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fModelName, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fSerialNumber, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fHardwareVersion, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fActiveFirmwareVersion, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fStagedFirmwareVersion, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pCallbacks->fLastError, exit, status = kUARPStatusInvalidFunctionPointer );

    /* clear everything up */
    memset( pAccessory, 0, sizeof( struct uarpPlatformAccessory ) );

    /* store the delegate */
    pAccessory->callbacks = *pCallbacks;
    pAccessory->pDelegate = pDelegate;
    pAccessory->pVendorExtension = pVendorExtension;
    
    /* setup callbacks */
    callbacks.fRequestTransmitMsgBuffer = uarpPlatformRequestTransmitMsgBuffer;
    callbacks.fReturnTransmitMsgBuffer = uarpPlatformReturnTransmitMsgBuffer;
    callbacks.fSendMessage = uarpPlatformSendMessage;
    callbacks.fAccessoryQueryAccessoryInfo = uarpPlatformQueryAccessoryInfo;
    callbacks.fAccessoryAssetOffered = uarpPlatformAssetOffered;
    callbacks.fAssetRescinded = uarpPlatformAssetRescinded;
    callbacks.fAccessoryAssetDataResponse = uarpPlatformAssetDataResponse;
    callbacks.fUpdateDataTransferPause = uarpPlatformDataTransferPause;
    callbacks.fUpdateDataTransferResume = uarpPlatformDataTransferResume;
    callbacks.fApplyStagedAssets = uarpPlatformApplyStagedAssets;
    callbacks.fVendorSpecific = fVendorSpecific;
    
    /* tell the lower edge we are here */
    pAccessory->_options = *pOptions;
    
    status = uarpAccessoryInit( &(pAccessory->_accessory), &callbacks, (void *)pAccessory );

__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformControllerAdd( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformController *pController,
                                   void *pControllerDelegate )
{
    uint32_t status;

    /* uarpAccessoryRemoteControllerAdd() needs this set before it is called */
    pController->pDelegate = pControllerDelegate;

    /* tell the lower layer about the controller */
    status = uarpAccessoryRemoteControllerAdd( &(pAccessory->_accessory),
                                              &(pController->_controller),
                                              (void *)pController );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Add Remote UARP Controller %d",
                pController->_controller.remoteControllerID );

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformControllerRemove( struct uarpPlatformAccessory *pAccessory,
                                      struct uarpPlatformController *pController )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Remove Remote UARP Controller %d",
                pController->_controller.remoteControllerID );

    /* tell the lower layer we are removing the contoller */
    status = uarpAccessoryRemoteControllerRemove( &(pAccessory->_accessory),
                                                 &(pController->_controller) );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );
    
    uarpPlatformCleanupAssetsForController( pAccessory, pController );
    
    /* done */
    status = kUARPStatusSuccess;

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryRecvMessage( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformController *pController,
                                          uint8_t *pBuffer, uint32_t length )
{
    uint32_t status;
    
    /* Verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );

    uarpLogDebug( kUARPLoggingCategoryPlatform, "RECV %u bytes from Remote UARP Controller %d",
                 length, pController->_controller.remoteControllerID );

    /* let the lower layer know we got a message */
    status = uarpAccessoryRecvMessage( &(pAccessory->_accessory), (void *)pController, pBuffer, length );

__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetIsAcceptable( struct uarpPlatformAccessory *pAccessory,
                                                struct uarpPlatformAsset *pAsset,
                                                UARPBool *pIsAcceptable )
{
    uint32_t status;
    struct UARPVersion activeFwVersion;
    struct UARPVersion stagedFwVersion;
    UARPVersionComparisonResult compareResult;

    /* assume the asset is acceptable until proven otherwise */
    *pIsAcceptable = kUARPYes;

    /* make sure the offer is a newer version than we are running */
    status = pAccessory->callbacks.fActiveFirmwareVersion( pAccessory->pDelegate,
                                                          pAsset->core.assetTag,
                                                          &activeFwVersion );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    compareResult = uarpVersionCompare( &activeFwVersion, &(pAsset->core.assetVersion) );
    
    if ( compareResult != kUARPVersionComparisonResultIsNewer )
    {
        uarpLogInfo( kUARPLoggingCategoryPlatform, "Active Firmware version is newer than the offered asset" );
        
        *pIsAcceptable = kUARPNo;
    }
    __UARP_Require_Action_Quiet( ( *pIsAcceptable == kUARPYes ), exit, status = kUARPStatusSuccess );

    /* make sure we don't have this version or newer staged */
    status = pAccessory->callbacks.fStagedFirmwareVersion( pAccessory->pDelegate,
                                                          pAsset->core.assetTag,
                                                          &stagedFwVersion );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    compareResult = uarpVersionCompare( &stagedFwVersion, &(pAsset->core.assetVersion) );
    
    if ( compareResult != kUARPVersionComparisonResultIsNewer )
    {
        uarpLogInfo( kUARPLoggingCategoryPlatform, "Staged Firmware version is newer than the offered asset" );

        *pIsAcceptable = kUARPNo;
    }
    __UARP_Require_Action_Quiet( ( *pIsAcceptable == kUARPYes ), exit, status = kUARPStatusSuccess );

    status = kUARPStatusSuccess;

exit:
    if ( status != kUARPStatusSuccess )
    {
        *pIsAcceptable = kUARPNo;
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetCookieIsAcceptable( struct uarpPlatformAccessory *pAccessory,
                                                      struct uarpPlatformAsset *pAsset,
                                                      struct uarpPlatformAssetCookie *pCookie,
                                                      UARPBool *pIsAcceptable )
{
    UARPVersionComparisonResult versionResult;
    
    versionResult = uarpVersionCompare( &(pAsset->core.assetVersion), &(pCookie->assetVersion) );
    
    if ( pAsset->core.assetTag != pCookie->assetTag )
    {
        *pIsAcceptable = kUARPNo;
    }
    else if ( versionResult != kUARPVersionComparisonResultIsEqual )
    {
        *pIsAcceptable = kUARPNo;
    }
    else if ( pAsset->core.assetTotalLength != pCookie->assetTotalLength )
    {
        *pIsAcceptable = kUARPNo;
    }
    else if ( pAsset->core.assetNumPayloads != pCookie->assetNumPayloads )
    {
        *pIsAcceptable = kUARPNo;
    }
    else
    {
        *pIsAcceptable = kUARPYes;
    }
    
    return kUARPStatusSuccess;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetAccept( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformController *pController,
                                          struct uarpPlatformAsset *pAsset )
{
    UARPBool isPresent;
    uint8_t flags;
    uint32_t status;
    struct uarpPlatformAsset *pAssetTmp;
    
    /* verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    /* add to our list, after making sure we aren't already on it.
     We could be on it because we are merging / resuming */
    isPresent = kUARPNo;
    
    for ( pAssetTmp = pAccessory->pAssetList; pAssetTmp; pAssetTmp = pAssetTmp->pNext )
    {
        if ( pAssetTmp == pAsset )
        {
            isPresent = kUARPYes;
            break;
        }
    }
    if ( isPresent == kUARPNo )
    {
        pAsset->pNext = pAccessory->pAssetList;
        pAccessory->pAssetList = pAsset;
    }
    
    pAsset->pausedByAccessory = kUARPNo;

    /* setup the scratch buffers for this asset, we may be merging an asset that already has a payload window */
    if ( pAsset->lengthScratchBuffer != pAccessory->_options.payloadWindowLength )
    {
        pAsset->lengthScratchBuffer = pAccessory->_options.payloadWindowLength;

        if ( pAsset->pScratchBuffer )
        {
            pAccessory->callbacks.fReturnBuffer( pAccessory->pDelegate, pAsset->pScratchBuffer );
            pAsset->pScratchBuffer = NULL;
        }
    }
    
    if ( pAsset->pScratchBuffer == NULL )
    {
        status = pAccessory->callbacks.fRequestBuffer( pAccessory->pDelegate, &(pAsset->pScratchBuffer),
                                                      pAsset->lengthScratchBuffer );
        __UARP_Require( ( status == kUARPStatusSuccess ), exit );
    }
    
    /* read the SuperBinary Header */
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Flags <%02x>", pAsset->internalFlags );
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Selected Payload %d, Flags <%02x>",
                pAsset->selectedPayloadIndex, pAsset->payload.internalFlags );

    /* determine what we need to do for pulling the asset, this is because of resuming an orphaned asset */
    flags = kUARPAssetHasHeader | kUARPAssetHasPayloadHeader;
    
    if ( ( pAsset->internalFlags & flags ) == 0 )
    {
        status = uarpPlatformAccessoryAssetSuperBinaryPullHeader( pAccessory, pAsset );
    }
    else if ( uarpPlatformAccessoryShouldRequestMetadata( pAsset->internalFlags ) == kUARPYes )
    {
        status = uarpPlatformAccessoryAssetRequestMetaData( pAccessory, pAsset );
    }
    else if ( pAsset->selectedPayloadIndex == kUARPPayloadIndexInvalid )
    {
        pAccessory->callbacks.fAssetMetaDataComplete( pAccessory->pDelegate, pAsset->pDelegate );
        
        status = kUARPStatusSuccess;
    }
    else if ( uarpPlatformAccessoryShouldRequestMetadata( pAsset->payload.internalFlags ) == kUARPYes )
    {
        pAccessory->callbacks.fPayloadReady( pAccessory->pDelegate, pAsset->pDelegate );
        
        status = kUARPStatusSuccess;
    }
    else
    {
        status = uarpPlatformAccessoryPayloadRequestData( pAccessory, pAsset );
    }

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetDeny( struct uarpPlatformAccessory *pAccessory,
                                        struct uarpPlatformController *pController,
                                        struct uarpPlatformAsset *pAsset )
{
    uint32_t status;
    
    /* verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pController, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    uarpLogDebug( kUARPLoggingCategoryPlatform, "Deny Asset ID <%u> for Controller <%d>",
                 pAsset->core.assetID, pController->_controller.remoteControllerID );

    status = uarpAccessoryAssetDeny( &(pAccessory->_accessory),
                                    (void *)pController,
                                    pAsset->core.assetID );
    if ( status == kUARPStatusSuccess )
    {
        pAsset->internalFlags |= kUARPAssetMarkForCleanup;
    
        pAsset->pDelegate = NULL;
    }
    
__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetAbandon( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformController *pController,
                                           struct uarpPlatformAsset *pAsset )
{
    uint32_t status;
    
    status = uarpPlatformAccessoryAssetAbandonInternal( pAccessory, pController, pAsset, kUARPYes );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetRelease( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformController *pController,
                                           struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    status = uarpPlatformAccessoryAssetAbandonInternal( pAccessory, pController, pAsset, kUARPNo );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetRequestMetaData( struct uarpPlatformAccessory *pAccessory,
                                                   struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    if ( pAsset && ( pAsset->sbHdr.superBinaryMetadataLength > 0 ) )
    {
        status = uarpPlatformAssetRequestData( pAccessory, pAsset,
                                              kUARPDataRequestTypeSuperBinaryMetadata,
                                              0,
                                              pAsset->sbHdr.superBinaryMetadataLength );
    }
    else
    {
        status = kUARPStatusNoMetaData;
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetSetPayloadIndex( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformAsset *pAsset, int payloadIdx )
{
    return uarpPlatformAssetSetPayloadIndexWithCookie( pAccessory, pAsset, payloadIdx, NULL );
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetSetPayloadIndexWithCookie( struct uarpPlatformAccessory *pAccessory,
                                                    struct uarpPlatformAsset *pAsset, int payloadIdx,
                                                    struct uarpPlatformAssetCookie *pCookie )
{
    uint32_t offset;
    uint32_t status;
    UARPBool isValidCookie;
    
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );
    __UARP_Require_Action( ( payloadIdx < pAsset->core.assetNumPayloads ), exit,
                          status = kUARPStatusInvalidArgument );

    isValidCookie = uarpPlatformAssetIsCookieValid( pAccessory, pAsset, pCookie );

    if ( isValidCookie )
    {
        payloadIdx = pCookie->selectedPayloadIndex;
    }
    
    pAsset->lengthPayloadRecvd = 0;
    
    /* save off the index and clear the fact that we have the paylaod header */
    pAsset->selectedPayloadIndex = payloadIdx;

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Set Active Payload Index <%d>", pAsset->selectedPayloadIndex );

    pAsset->internalFlags = pAsset->internalFlags & ~kUARPAssetHasPayloadHeader;
    pAsset->payload.internalFlags = 0;

    /* determine offset based on selected index payloadIdx */
    offset = pAsset->selectedPayloadIndex * sizeof( struct UARPPayloadHeader );

    /* call the lower edge */
    status = uarpPlatformAssetRequestData( pAccessory, pAsset,
                                          kUARPDataRequestTypeSuperBinaryPayloadHeader,
                                          offset,
                                          sizeof( struct UARPPayloadHeader ) );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryPayloadRequestMetaData( struct uarpPlatformAccessory *pAccessory,
                                                     struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    if ( pAsset && ( pAsset->payload.plHdr.payloadMetadataLength > 0 ) )
    {
        status = uarpPlatformAssetRequestData( pAccessory, pAsset,
                                              kUARPDataRequestTypePayloadMetadata,
                                              0,
                                              pAsset->payload.plHdr.payloadMetadataLength );
    }
    else
    {
        status = kUARPStatusNoMetaData;
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetSetPayloadOffset( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformAsset *pAsset,
                                           uint32_t payloadOffset )
{
    uint32_t status;
    
    /* ensure that...
        - we have selected a payload
        - the offset is within bounds
        - there is not an outstanding data request
     */
    if ( pAsset == NULL )
    {
        status = kUARPStatusInvalidArgument;
    }
    else if ( pAsset->selectedPayloadIndex == kUARPPayloadIndexInvalid )
    {
        status = kUARPStatusInvalidPayload;
    }
    else if ( payloadOffset >= pAsset->payload.plHdr.payloadLength )
    {
        status = kUARPStatusInvalidOffset;
    }
    else if ( pAsset->dataReq.requestType & kUARPDataRequestTypeOutstanding )
    {
        status = kUARPStatusAssetInFlight;
    }
    else
    {
        pAsset->lengthPayloadRecvd = payloadOffset;

        status = kUARPStatusSuccess;
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryPayloadRequestData( struct uarpPlatformAccessory *pAccessory,
                                                 struct uarpPlatformAsset *pAsset )
{
    return uarpPlatformAccessoryPayloadRequestDataWithCookie( pAccessory, pAsset, NULL );
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryPayloadRequestDataWithCookie( struct uarpPlatformAccessory *pAccessory,
                                                           struct uarpPlatformAsset *pAsset,
                                                           struct uarpPlatformAssetCookie *pCookie )
{
    UARPBool isValidCookie;
    uint32_t length;
    uint32_t status;

    /* verifiers */
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    /* if we have a valid cookie and try to update the payload offset */
    isValidCookie = uarpPlatformAssetIsCookieValid( pAccessory, pAsset, pCookie );
    
    if ( isValidCookie == kUARPYes )
    {
        status = uarpPlatformAssetSetPayloadOffset( pAccessory, pAsset, pCookie->lengthPayloadRecvd );
        __UARP_Check( status == kUARPStatusSuccess );
    }
    
    /* payload window unless less is left */
    length = pAsset->payload.plHdr.payloadLength - pAsset->lengthPayloadRecvd;
    
    if ( length > pAsset->lengthScratchBuffer )
    {
        length = pAsset->lengthScratchBuffer;
    }
    
    /* call the lower edge */
    status = uarpPlatformAssetRequestData( pAccessory, pAsset,
                                          kUARPDataRequestTypePayloadPayload,
                                          pAsset->lengthPayloadRecvd,
                                          length );
    
__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryPayloadRequestDataPause( struct uarpPlatformAccessory *pAccessory,
                                                      struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    __UARP_Require_Action( ( pAsset->pausedByAccessory == kUARPNo ), exit, status = kUARPStatusSuccess );

    pAsset->pausedByAccessory = kUARPYes;

    uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Transfer paused on Asset ID %u, by accessory request",
                 pAsset->core.assetID );

    /* NOTE: Intentionally do not try to pause an outstanding data request.  Let it complete. */
    status = kUARPStatusSuccess;

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryPayloadRequestDataResume( struct uarpPlatformAccessory *pAccessory,
                                                       struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    __UARP_Require_Action( ( pAsset->pausedByAccessory == kUARPYes ), exit, status = kUARPStatusSuccess );

    pAsset->pausedByAccessory = kUARPNo;

    uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Transfer resumed on Asset ID %u, by accessory request",
                 pAsset->core.assetID );

    __UARP_Require_Action( ( pAsset->pController != NULL ), exit, status = kUARPStatusSuccess );

    if ( ( pAsset->dataReq.requestType & kUARPDataRequestTypeOutstanding ) == 0 )
    {
        status = uarpPlatformAccessoryPayloadRequestData( pAccessory, pAsset );
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetFullyStaged( struct uarpPlatformAccessory *pAccessory,
                                               struct uarpPlatformAsset *pAsset )
{
    uint32_t status;
    
    /* verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );
    __UARP_Require_Action( pAsset->pController, exit, status = kUARPStatusInvalidArgument );

    uarpLogDebug( kUARPLoggingCategoryPlatform, "Staged Asset ID <%u> for Controller <%d>",
                 pAsset->core.assetID,
                 pAsset->pController->_controller.remoteControllerID );

    status = uarpAccessoryAssetStaged( &(pAccessory->_accessory),
                                      pAsset->pController->_controller.pDelegate,
                                      pAsset->core.assetID );

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessorySuperBinaryMerge( struct uarpPlatformAccessory *pAccessory,
                                               struct uarpPlatformAsset *pAssetOrphaned,
                                               struct uarpPlatformAsset *pAssetOffered )
{
    uint32_t status;
    
    /* verifiers */
    __UARP_Verify_Action( pAssetOrphaned, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pAssetOffered, exit, status = kUARPStatusInvalidArgument );

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Merging Assets <%u> -> <%u>",
                pAssetOffered->core.assetID, pAssetOrphaned->core.assetID );
    
    pAssetOrphaned->core.assetID = pAssetOffered->core.assetID;

    pAssetOrphaned->dataReq.requestType &= ~kUARPDataRequestTypeOutstanding;
    
    pAssetOffered->internalFlags |= kUARPAssetMarkForCleanup;
    
    pAssetOrphaned->pController = pAssetOffered->pController;
    pAssetOffered->pController = NULL;
    
    /* set the delegate for the platform */
    pAssetOrphaned->pDelegate = pAssetOrphaned;
    
    /* done */
    status = kUARPStatusSuccess;
    
__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformAccessorySendMessageComplete( struct uarpPlatformAccessory *pAccessory,
                                              struct uarpPlatformController *pController,
                                              uint8_t *pBuffer )
{
    if ( pAccessory )
    {
        uarpPlatformReturnTransmitMsgBuffer( pAccessory, pController, pBuffer );
    }

    return;
}

/* MARK: HELPERS */

/* -------------------------------------------------------------------------------- */

struct uarpPlatformAsset *
    uarpPlatformAssetFindByAssetID( struct uarpPlatformAccessory *pAccessory,
                                   struct uarpPlatformController *pController,
                                   uint16_t assetID )
{
    struct uarpPlatformAsset *pTmp;
    struct uarpPlatformAsset *pAsset;

    pAsset = NULL;

    /* look for the remote accessory in the superbinary list */
    for ( pTmp = pAccessory->pAssetList; pTmp; pTmp = pTmp->pNext  )
    {
        if ( ( pTmp->pController == pController ) && ( pTmp->core.assetID == assetID ) )
        {
            pAsset = pTmp;
            break;
        }
    }

    return pAsset;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformUpdateSuperBinaryMetaData( struct uarpPlatformAccessory *pAccessory,
                                               struct uarpPlatformAsset *pAsset,
                                               void *pBuffer, uint32_t lengthBuffer )
{
    uint32_t status;

    /* mark as having the metadata */
    pAsset->internalFlags |= kUARPAssetHasMetadata;
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "SuperBinary MetaData Rx COMPLETE" );

    status = uarpPlatformUpdateMetaData( pAccessory, pAsset, pBuffer, lengthBuffer,
                                        pAccessory->callbacks.fAssetMetaDataTLV,
                                        pAccessory->callbacks.fAssetMetaDataComplete );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformUpdatePayloadMetaData( struct uarpPlatformAccessory *pAccessory,
                                           struct uarpPlatformAsset *pAsset,
                                           void *pBuffer, uint32_t lengthBuffer )
{
    uint32_t status;
    struct uarpPayloadObj *pPayload;
    
    /* mark as having the metadata */
    pPayload = &(pAsset->payload);
    pPayload->internalFlags |= kUARPAssetHasMetadata;
        
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset <%d> Payload <%c%c%c%c> MetaData Rx COMPLETE",
                pAsset->core.assetID,
                pPayload->payload4cc[0], pPayload->payload4cc[1],
                pPayload->payload4cc[2], pPayload->payload4cc[3] );
    
    status = uarpPlatformUpdateMetaData( pAccessory, pAsset, pBuffer, lengthBuffer,
                                        pAccessory->callbacks.fPayloadMetaDataTLV,
                                        pAccessory->callbacks.fPayloadMetaDataComplete );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformUpdatePayloadPayload( struct uarpPlatformAccessory *pAccessory,
                                          struct uarpPlatformAsset *pAsset,
                                          void *pBuffer, uint32_t lengthBuffer )
{
    uint32_t status;
    struct uarpPayloadObj *pPayload;
    struct uarpPlatformAssetCookie cookie;
    
    /* need to update what was coming from controller */
    pPayload = &(pAsset->payload);

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Payload <%c%c%c%c> Payload Rx Window Complete %u bytes from offset %u",
                pPayload->payload4cc[0], pPayload->payload4cc[1],
                pPayload->payload4cc[2], pPayload->payload4cc[3],
                lengthBuffer, pAsset->lengthPayloadRecvd );

    cookie.assetTag = pAsset->core.assetTag;
    cookie.assetVersion = pAsset->core.assetVersion;
    cookie.assetTotalLength = pAsset->core.assetTotalLength;
    cookie.assetNumPayloads = pAsset->core.assetNumPayloads;
    cookie.selectedPayloadIndex = pAsset->selectedPayloadIndex;
    cookie.lengthPayloadRecvd = pAsset->lengthPayloadRecvd;

    pAccessory->callbacks.fPayloadData( pAccessory->pDelegate, pAsset->pDelegate, pBuffer, lengthBuffer,
                                       pAsset->lengthPayloadRecvd, (void *)&cookie, sizeof( cookie ) );

    pAsset->lengthPayloadRecvd += lengthBuffer;
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Payload <%c%c%c%c> Payload RX %u bytes of %u",
                pPayload->payload4cc[0], pPayload->payload4cc[1],
                pPayload->payload4cc[2], pPayload->payload4cc[3],
                pAsset->lengthPayloadRecvd, pPayload->plHdr.payloadLength );

    /* either pull the next window or indicate this is completed */
    if ( pAsset->lengthPayloadRecvd == pPayload->plHdr.payloadLength )
    {
        pPayload->internalFlags |= kUARPAssetHasPayload;

        uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Payload <%c%c%c%c> Payload Rx COMPLETE",
                    pPayload->payload4cc[0], pPayload->payload4cc[1],
                    pPayload->payload4cc[2], pPayload->payload4cc[3] );
        
        pAccessory->callbacks.fPayloadDataComplete( pAccessory->pDelegate, pAsset->pDelegate );
        
        status = kUARPStatusSuccess;
    }
    else if ( pAsset->pausedByAccessory == kUARPNo )
    {
        status = uarpPlatformAccessoryPayloadRequestData( pAccessory, pAsset );
    }
    else
    {
        status = kUARPStatusSuccess;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformCleanupAssetsForController( struct uarpPlatformAccessory *pAccessory,
                                            struct uarpPlatformController *pController )
{
    struct uarpPlatformAsset *pAssets;
    struct uarpPlatformAsset *pTmpAsset;
    
    /* run through the assets, orphaning superbinaries from this controller. */
    pAssets = pAccessory->pAssetList;
    pAccessory->pAssetList = NULL;

    while ( pAssets )
    {
        pTmpAsset = pAssets;
        pAssets = pAssets->pNext;
        
        /* different controller and we can just continue */
        if ( pTmpAsset->pController != pController )
        {
            pTmpAsset->pNext = pAccessory->pAssetList;
            pAccessory->pAssetList = pTmpAsset;
            continue;
        }

        /* If we're a dynamic asset, we need to be released */
        if ( uarpAssetIsDynamicAsset( &pTmpAsset->core ) )
        {
            pTmpAsset->internalFlags |= kUARPAssetMarkForCleanup;
        }

        if ( pTmpAsset->internalFlags & kUARPAssetMarkForCleanup )
        {
            uarpPlatformAssetRelease( pAccessory, pTmpAsset );

            uarpPlatformAssetCleanup( pAccessory, pTmpAsset );
            
            continue;
        }
        
        /* if we're a superbinary, let the upper layer know we were orphaned and keep it around */
        uarpPlatformAssetOrphan( pAccessory, pTmpAsset );
        
        pTmpAsset->pNext = pAccessory->pAssetList;
        pAccessory->pAssetList = pTmpAsset;
    }

    return;
}
    
/* -------------------------------------------------------------------------------- */

void uarpPlatformCleanupAssets( struct uarpPlatformAccessory *pAccessory )
{
    uarpPlatformCleanupAssetsForController( pAccessory, NULL );
    
    return;
}
    
/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformUpdateMetaData( struct uarpPlatformAccessory *pAccessory,
                                    struct uarpPlatformAsset *pAsset,
                                    void *pBuffer, uint32_t lengthBuffer,
                                    fcnUarpPlatformAccessoryMetaDataTLV fMetaDataTLV,
                                    fcnUarpPlatformAccessoryMetaDataComplete fMetaDataComplete )
{
    uint8_t *pTmp;
    uint32_t tlvType;
    uint32_t tlvLength;
    uint32_t status;
    uint32_t remainingLength;
    struct UARPTLVHeader *pTlv;

    /* mark as having the metadata */
    pAsset->internalFlags |= kUARPAssetHasMetadata;
    
    /* iterate */
    pTmp = pBuffer;
    remainingLength = lengthBuffer;
    
    while ( remainingLength >= sizeof( struct UARPTLVHeader ) )
    {
        pTlv = (struct UARPTLVHeader *)pTmp;
        
        tlvType = uarpNtohl( pTlv->tlvType );
        tlvLength = uarpNtohl( pTlv->tlvLength );
        
        pTmp += sizeof( struct UARPTLVHeader );
        remainingLength -= sizeof( struct UARPTLVHeader );
        
        __UARP_Require_Action( ( remainingLength >= tlvLength ), exit, status = kUARPStatusMetaDataCorrupt );

        fMetaDataTLV( pAccessory->pDelegate, pAsset->pDelegate, tlvType, tlvLength, pTmp );

        pTmp += tlvLength;
        remainingLength -= tlvLength;
    }

    fMetaDataComplete( pAccessory->pDelegate, pAsset->pDelegate );

    /* done */
    status = kUARPStatusSuccess;

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetSuperBinaryPullHeader( struct uarpPlatformAccessory *pAccessory,
                                                         struct uarpPlatformAsset *pAsset )
{
    uint32_t status;

    /* verifiers */
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );

    /* call the lower edge */
    status = uarpPlatformAssetRequestData( pAccessory, pAsset,
                                          kUARPDataRequestTypeSuperBinaryHeader,
                                          0,
                                          sizeof( struct UARPSuperBinaryHeader ) );
    
__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

UARPBool uarpPlatformAccessoryShouldRequestMetadata( uint8_t flags )
{
    UARPBool status;
    
    if ( ( flags & kUARPAssetNeedsMetadata ) && ( ( flags & kUARPAssetHasMetadata ) == 0 ) )
    {
        status = kUARPYes;
    }
    else
    {
        status = kUARPNo;
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAccessoryAssetAbandonInternal( struct uarpPlatformAccessory *pAccessory,
                                                   struct uarpPlatformController *pController,
                                                   struct uarpPlatformAsset *pAsset,
                                                   UARPBool notifyController )
{
    uint32_t status;
    
    if ( pAsset )
    {
        uarpLogDebug( kUARPLoggingCategoryPlatform, "Abandon Asset ID <%u> for Controller <%d>",
                     pAsset->core.assetID,
                     pController ? pController->_controller.remoteControllerID : -1 );

        if ( notifyController == kUARPYes )
        {
            status = uarpAccessoryAssetAbandon( &(pAccessory->_accessory),
                                               pController,
                                               pAsset->core.assetID );
        }
        else
        {
            status = kUARPStatusSuccess;
        }

        pAsset->dataReq.requestType &= ~kUARPDataRequestTypeOutstanding;

        pAsset->internalFlags |= kUARPAssetMarkForCleanup;
        
        pAsset->pController = NULL;
    }
    else
    {
        status = kUARPStatusSuccess;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformSuperBinaryHeaderDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                                          struct uarpPlatformAsset *pAsset,
                                                           uint8_t reqType, uint32_t payloadTag,
                                                           uint32_t offset, uint8_t * pBuffer, uint32_t length )
{
    uint32_t status;
    struct UARPSuperBinaryHeader *pSbHdr;

    /* alias the request buffer and superbinary header */
    pSbHdr = (struct UARPSuperBinaryHeader *)pBuffer;

    pAsset->sbHdr.superBinaryFormatVersion = uarpNtohl( pSbHdr->superBinaryFormatVersion );
    pAsset->sbHdr.superBinaryHeaderLength = uarpNtohl( pSbHdr->superBinaryHeaderLength );
    pAsset->sbHdr.superBinaryLength = uarpNtohl( pSbHdr->superBinaryLength );
    
    uarpVersionEndianSwap( &(pSbHdr->superBinaryVersion), &(pAsset->sbHdr.superBinaryVersion) );
    
    pAsset->sbHdr.superBinaryMetadataOffset = uarpNtohl( pSbHdr->superBinaryMetadataOffset );
    pAsset->sbHdr.superBinaryMetadataLength = uarpNtohl( pSbHdr->superBinaryMetadataLength );
    pAsset->sbHdr.payloadHeadersOffset = uarpNtohl( pSbHdr->payloadHeadersOffset );
    pAsset->sbHdr.payloadHeadersLength = uarpNtohl( pSbHdr->payloadHeadersLength );

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Offered (asset id %u)",
                pAsset->core.assetID );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Format Version %08x",
                pAsset->sbHdr.superBinaryFormatVersion );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Header Length %u",
                pAsset->sbHdr.superBinaryHeaderLength );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Length %u",
                pAsset->sbHdr.superBinaryLength );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Version (%u.%u.%u.%u)",
                pAsset->sbHdr.superBinaryVersion.major,
                pAsset->sbHdr.superBinaryVersion.minor,
                pAsset->sbHdr.superBinaryVersion.release,
                pAsset->sbHdr.superBinaryVersion.build );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Metadata Offset %u",
                pAsset->sbHdr.superBinaryMetadataOffset );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Metadata Length %u",
                pAsset->sbHdr.superBinaryMetadataLength );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Headers Offset %u",
                pAsset->sbHdr.payloadHeadersOffset );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Headers Length %u",
                pAsset->sbHdr.payloadHeadersLength );
    
    /* Verify lengths and offset */
    if ( pAsset->sbHdr.superBinaryFormatVersion != kUARPSuperBinaryFormatVersion )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else if ( pAsset->sbHdr.superBinaryHeaderLength != sizeof( struct UARPSuperBinaryHeader ) )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else if ( ( pAsset->sbHdr.superBinaryMetadataOffset + pAsset->sbHdr.superBinaryMetadataLength ) >
             pAsset->core.assetTotalLength )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else if ( ( pAsset->sbHdr.payloadHeadersOffset + pAsset->sbHdr.payloadHeadersLength ) >
             pAsset->core.assetTotalLength )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    
    if ( status != kUARPStatusSuccess )
    {
        status = uarpAccessoryAssetCorrupt( &(pAccessory->_accessory), pAsset->pController->_controller.pDelegate,
                                           pAsset->core.assetID );

        pAccessory->callbacks.fAssetCorrupt( pAccessory->pDelegate, pAsset->pDelegate );
    }
    else
    {
        if ( pAsset->sbHdr.superBinaryMetadataLength > 0 )
        {
            pAsset->internalFlags |= kUARPAssetNeedsMetadata;
        }
        
        pAsset->internalFlags |= kUARPAssetHasHeader;

        status = uarpPlatformDataRequestComplete( pAccessory, pAsset, reqType, payloadTag, 0, NULL, 0 );
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetPayloadHeaderDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                                           struct uarpPlatformAsset *pAsset,
                                                           uint8_t reqType, uint32_t payloadTag,
                                                           uint32_t offset, uint8_t * pBuffer, uint32_t length )
{
    uint32_t status;
    struct UARPPayloadHeader *pPlHdr;
    
    /* TODO: double check reqType */

    /* alias the request buffer and superbinary header */
    pPlHdr = (struct UARPPayloadHeader *)pBuffer;
    
    pAsset->payload.plHdr.payloadHeaderLength = uarpNtohl( pPlHdr->payloadHeaderLength );
    pAsset->payload.plHdr.payloadTag = pPlHdr->payloadTag; /* no endian intentionally */
    
    uarpVersionEndianSwap( &(pPlHdr->payloadVersion), &(pAsset->payload.plHdr.payloadVersion) );
    
    pAsset->payload.plHdr.payloadMetadataOffset = uarpNtohl( pPlHdr->payloadMetadataOffset );
    pAsset->payload.plHdr.payloadMetadataLength = uarpNtohl( pPlHdr->payloadMetadataLength );
    pAsset->payload.plHdr.payloadOffset = uarpNtohl( pPlHdr->payloadOffset );
    pAsset->payload.plHdr.payloadLength = uarpNtohl( pPlHdr->payloadLength );
    
    pAsset->payload.internalFlags |= kUARPAssetHasPayloadHeader;
    
    if ( pAsset->payload.plHdr.payloadMetadataLength > 0 )
    {
        pAsset->payload.internalFlags |= kUARPAssetNeedsMetadata;
    }

    uarpPayloadTagUnpack( pAsset->payload.plHdr.payloadTag, pAsset->payload.payload4cc );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Offered (asset id %u), Payload %d",
                pAsset->core.assetID, pAsset->selectedPayloadIndex );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Header Length %u",
                pAsset->payload.plHdr.payloadHeaderLength );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Tag 0x%08x <%c%c%c%c>",
                pAsset->payload.plHdr.payloadTag,
                pAsset->payload.payload4cc[0],
                pAsset->payload.payload4cc[1],
                pAsset->payload.payload4cc[2],
                pAsset->payload.payload4cc[3]);
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Version (%u.%u.%u.%u)",
                pAsset->payload.plHdr.payloadVersion.major,
                pAsset->payload.plHdr.payloadVersion.minor,
                pAsset->payload.plHdr.payloadVersion.release,
                pAsset->payload.plHdr.payloadVersion.build );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Metadata Offset %u",
                pAsset->payload.plHdr.payloadMetadataOffset );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Metadata Length %u",
                pAsset->payload.plHdr.payloadMetadataLength );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Offset %u",
                pAsset->payload.plHdr.payloadOffset );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "- Payload Length %u",
                pAsset->payload.plHdr.payloadLength );
    
    /* Verify lengths and offset */
    if ( ( pAsset->payload.plHdr.payloadMetadataOffset + pAsset->payload.plHdr.payloadMetadataLength ) >
             pAsset->core.assetTotalLength )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else if ( ( pAsset->payload.plHdr.payloadOffset + pAsset->payload.plHdr.payloadLength ) >
             pAsset->core.assetTotalLength )
    {
        status = kUARPStatusInvalidSuperBinaryHeader;
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    
    if ( status != kUARPStatusSuccess )
    {
        status = uarpAccessoryAssetCorrupt( &(pAccessory->_accessory), pAsset->pController->_controller.pDelegate,
                                           pAsset->core.assetID );

        pAccessory->callbacks.fAssetCorrupt( pAccessory->pDelegate, pAsset->pDelegate );
    }
    else
    {
        pAsset->internalFlags |= kUARPAssetHasPayloadHeader;
    
        status = uarpPlatformDataRequestComplete( pAccessory, pAsset, reqType, payloadTag, 0, NULL, 0 );
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetRequestDataContinue( struct uarpPlatformAccessory *pAccessory,
                                              struct uarpPlatformController *pController,
                                              struct uarpPlatformAsset *pAsset )
{
    uint32_t status;
    uint32_t bytesToRequest;

    /* TODO: make quiet */
    __UARP_Require_Action( ( pAsset->pausedByAccessory == kUARPNo ), exit, status = kUARPStatusDataTransferPaused );
    __UARP_Require_Action( ( pAsset->dataReq.bytesRemaining > 0 ), exit, status = kUARPStatusAssetNoBytesRemaining );
        
    /* adjust bytes to request */
    bytesToRequest = pAsset->dataReq.bytesRemaining;
    
    if ( bytesToRequest > pAccessory->_options.maxRxPayloadLength )
    {
        bytesToRequest = pAccessory->_options.maxRxPayloadLength;
    }
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "REQ BYTES - Asset <%u> <%c%c%c%c> Request Type <0x%x> ",
                pAsset->core.assetID,
                pAsset->payload.payload4cc[0] ? pAsset->payload.payload4cc[0] : '0',
                pAsset->payload.payload4cc[1] ? pAsset->payload.payload4cc[1] : '0',
                pAsset->payload.payload4cc[2] ? pAsset->payload.payload4cc[2] : '0',
                pAsset->payload.payload4cc[3] ? pAsset->payload.payload4cc[3] : '0',
                pAsset->dataReq.requestType);
    uarpLogInfo( kUARPLoggingCategoryPlatform,
                "Relative Offset <%u> Absolute Offset <%u> Current Offset <%u> ",
                pAsset->dataReq.relativeOffset,
                pAsset->dataReq.absoluteOffset,
                pAsset->dataReq.currentOffset);
    uarpLogInfo( kUARPLoggingCategoryPlatform,
                "Bytes Requested <%u> Bytes Responded <%u> Bytes Remaining <%u> Bytes to Request <%u>",
                pAsset->dataReq.bytesRequested,
                pAsset->dataReq.bytesResponded,
                pAsset->dataReq.bytesRemaining,
                bytesToRequest );

    status = uarpAccessoryAssetRequestData( (void *)&(pAccessory->_accessory),
                                           (void *)pController,
                                           pAsset->core.assetID,
                                           pAsset->dataReq.currentOffset,
                                           bytesToRequest );
    
exit:
    /* TODO: is this appropriate here, given the require macros?? */
    if ( status == kUARPStatusSuccess )
    {
        pAsset->dataReq.requestType |= kUARPDataRequestTypeOutstanding;
    }
    else if ( status == kUARPStatusDataTransferPaused )
    {
        status = kUARPStatusSuccess;
    }
    else if ( status == kUARPStatusAssetNoBytesRemaining )
    {
        status = kUARPStatusSuccess;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */
/* pAsset will be released when returning from this routine */
void uarpPlatformAssetCleanup( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset )
{
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset cleaned up (asset id %u)",
                pAsset->core.assetID );
    
    if ( pAsset->pScratchBuffer )
    {
        pAccessory->callbacks.fReturnBuffer( pAccessory->pDelegate, (uint8_t *)pAsset->pScratchBuffer );
    }
    
    pAccessory->callbacks.fReturnBuffer( pAccessory->pDelegate, (uint8_t *)pAsset );

    return;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetOffered( void *pAccessoryDelegate, void *pControllerDelegate,
                                  struct uarpAssetCoreObj *pAssetCore )
{
    uint32_t status;
    struct uarpPlatformAsset *pAsset;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    
    /* initializers */
    pAsset = NULL;
    
    /* alias our context pointer */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    status = pAccessory->callbacks.fRequestBuffer( pAccessory->pDelegate, (uint8_t **)&pAsset,
                                                  sizeof( struct uarpPlatformAsset ) );
    __UARP_Require( status == kUARPStatusSuccess, exit );

    pAsset->pController = pController;

    pAsset->core = *pAssetCore;

    pAsset->selectedPayloadIndex = kUARPPayloadIndexInvalid;
    
    uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Offered from UARP Controller %d <Asset ID %u>",
                 pController->_controller.remoteControllerID,
                 pAsset->core.assetID );
    
    uarpLogDebug( kUARPLoggingCategoryPlatform, "- Version <%u.%u.%u.%u>",
                 pAsset->core.assetVersion.major,
                 pAsset->core.assetVersion.minor,
                 pAsset->core.assetVersion.release,
                 pAsset->core.assetVersion.build );
    
    uarpLogDebug( kUARPLoggingCategoryPlatform, "- Flags <0x%08x>", pAsset->core.assetFlags );
    
    uarpLogDebug( kUARPLoggingCategoryPlatform, "- Tag <0x%08x>", pAsset->core.assetTag );
    
    /* put this on the right asset list and indicate to the upper layer */
    pAsset->pNext = pAccessory->pAssetList;
    pAccessory->pAssetList = pAsset;

    /* Tell the upper layer about this asset being offered */
    if ( uarpAssetIsSuperBinary( &(pAsset->core) ) )
    {
        pAccessory->callbacks.fSuperBinaryOffered( pAccessory->pDelegate, pController->pDelegate, pAsset );

        status = kUARPStatusSuccess;
    }
    else if ( uarpAssetIsDynamicAsset( &(pAsset->core) ) )
    {
        pAccessory->callbacks.fDynamicAssetOffered( pAccessory->pDelegate, pController->pDelegate, pAsset );

        status = kUARPStatusSuccess;
    }
    else
    {
        status = kUARPStatusInvalidAssetType;
    }

exit:
    if ( status == kUARPStatusSuccess )
    {
        uarpPlatformCleanupAssetsForController( pAccessory, NULL );
    }
    /* TODO: if failed, free asset and payloads */
    
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetDataResponse( void *pAccessoryDelegate, void *pControllerDelegate, uint16_t assetID,
                                        uint8_t *pBuffer, uint32_t length, uint32_t offset )
{
    uint8_t *pResponseBuffer;
    uint8_t payload4cc[kUARPSuperBinaryPayloadTagLength];
    uint32_t status;
    struct uarpPlatformAsset *pAsset;
    struct uarpDataRequestObj *pRequest;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    fcnUarpPlatformAssetDataRequestComplete fRequestComplete;
    
    /* alias our context pointers */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;
    
    pAsset = uarpPlatformAssetFindByAssetID( pAccessory, pController, assetID );
    __UARP_Require_Action( pAsset, exit, status = kUARPStatusNoResources );
    
    /* if the asset has been abandoned or rescinded, we should ignore this response */
    if ( pAsset->internalFlags & kUARPAssetMarkForCleanup )
    {
        pAsset = NULL;
    }
    __UARP_Require_Action( pAsset, exit, status = kUARPStatusUnknownAsset );

    /* copy into the data response buffer, if everything checks out */
    pRequest = &(pAsset->dataReq);
    __UARP_Require_Action( ( pRequest->currentOffset == offset ), exit, status = kUARPStatusMismatchDataOffset );
    __UARP_Require_Action( ( pRequest->bytesRequested >= length ), exit,
                          status = kUARPStatusInvalidDataResponseLength );
    __UARP_Require_Action( ( pRequest->requestType | kUARPDataRequestTypeOutstanding ), exit,
                          status = kUARPStatusInvalidDataResponse );
    
    uarpPayloadTagUnpack( pRequest->payloadTag, payload4cc );

    pResponseBuffer = pRequest->bytes;
    pResponseBuffer += pRequest->bytesResponded;

    /* copy buffers */
    memcpy( pResponseBuffer, pBuffer, length );
    
    pRequest->bytesResponded += length;

    pRequest->requestType = pRequest->requestType & ~kUARPDataRequestTypeOutstanding;

    /* some requests are internal to the Firmware Updater, so we will hijack the completion routine */
    if ( pRequest->requestType == kUARPDataRequestTypeSuperBinaryHeader )
    {
        fRequestComplete = uarpPlatformSuperBinaryHeaderDataRequestComplete;
    }
    else if ( pRequest->requestType == kUARPDataRequestTypeSuperBinaryPayloadHeader )
    {
        fRequestComplete = uarpPlatformAssetPayloadHeaderDataRequestComplete;
    }
    else
    {
        fRequestComplete = uarpPlatformDataRequestComplete;
    }
    
    /* inform the delegate when the request is full */
    /* TODO: Handle when we simply won't fill the requested buffer; or does bytesRequested handle that? */
    pRequest->bytesRemaining = pRequest->bytesRequested - pRequest->bytesResponded;
    
    uarpLogInfo( kUARPLoggingCategoryPlatform, "RSP BYTES - Asset <%u> <%c%c%c%c> Request Type <0x%x> ",
                pAsset->core.assetID,
                payload4cc[0], payload4cc[1], payload4cc[2], payload4cc[3], pRequest->requestType);
    
    uarpLogInfo( kUARPLoggingCategoryPlatform,
                "Relative Offset <%u> Absolute Offset <%u> Current Offset <%u> ",
                pRequest->relativeOffset, pRequest->absoluteOffset, pRequest->currentOffset );
    
    uarpLogInfo( kUARPLoggingCategoryPlatform,
                "Bytes Requested <%u> Bytes Responded <%u> Total Bytes Responded <%u> Bytes Remaining <%u> ",
                pRequest->bytesRequested, length, pRequest->bytesResponded, pRequest->bytesRemaining );


    pRequest->currentOffset = pRequest->absoluteOffset + pRequest->bytesResponded;

    /* TODO: do we want to hold onto a full payload if paused, until resume? */
    if ( pRequest->bytesResponded == pRequest->bytesRequested )
    {
        uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Data Response from Controller ID <%d> - All Bytes Requested",
                     pController->_controller.remoteControllerID );

        status = fRequestComplete( pAccessory, pAsset, pRequest->requestType,
                                  pRequest->payloadTag, pRequest->relativeOffset,
                                  pRequest->bytes, pRequest->bytesResponded );
    }
    else if ( pController->_controller.dataTransferAllowed == kUARPNo )
    {
        uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Data Response from Controller ID <%d>"
                     "Transfer Paused by controller, wait for resume",
                     pController->_controller.remoteControllerID );
        
        status = kUARPStatusSuccess;
    }
    else if ( pAsset->pausedByAccessory == kUARPYes )
    {
        uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Data Response from Controller ID <%d>"
                     "Transfer Paused by accessory, wait for resume",
                     pController->_controller.remoteControllerID );
        
        status = kUARPStatusSuccess;
    }
    else
    {
        status = uarpPlatformAssetRequestDataContinue( pAccessory, pAsset->pController, pAsset );
    }
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformAssetRequestData( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset,
                                      uint8_t requestType, uint32_t relativeOffset, uint32_t lengthNeeded )

{
    uint32_t status;
    uint32_t tmp32;
    uint32_t startOffset;
    uint32_t maxLength;
    struct uarpPayloadObj *pPayload;
    struct uarpDataRequestObj *pRequest;
    
    __UARP_Verify_Action( pAsset, exit, status = kUARPStatusInvalidArgument );
    __UARP_Require_Action( pAsset->pScratchBuffer, exit, status = kUARPStatusNoResources );
    __UARP_Require_Action( ( pAsset->lengthScratchBuffer >= lengthNeeded ), exit,
                          status = kUARPStatusNoResources );

    pRequest = &(pAsset->dataReq);
    __UARP_Require_Action( ( ( pRequest->requestType & kUARPDataRequestTypeOutstanding ) == 0 ), exit,
                          status = kUARPStatusAssetInFlight );

    pRequest->requestType = requestType;
    pRequest->relativeOffset = relativeOffset;
    pRequest->bytesRequested = lengthNeeded;
    pRequest->bytes = pAsset->pScratchBuffer;

    pRequest->bytesResponded = 0;

    /* Only one outstanding request per asset */
    if ( pRequest->requestType == kUARPDataRequestTypeSuperBinaryHeader )
    {
        startOffset = 0;
        maxLength = sizeof( struct UARPSuperBinaryHeader );
    }
    else if ( pRequest->requestType == kUARPDataRequestTypeSuperBinaryPayloadHeader )
    {
        startOffset = pAsset->sbHdr.payloadHeadersOffset;
        maxLength = pAsset->sbHdr.payloadHeadersLength;
    }
    else if ( pRequest->requestType == kUARPDataRequestTypeSuperBinaryMetadata )
    {
        startOffset = pAsset->sbHdr.superBinaryMetadataOffset;
        maxLength = pAsset->sbHdr.superBinaryMetadataLength;
    }
    else if ( pRequest->requestType == kUARPDataRequestTypePayloadMetadata )
    {
        pPayload = &(pAsset->payload);

        pRequest->payloadTag = pAsset->payload.plHdr.payloadTag;

        startOffset = pPayload->plHdr.payloadMetadataOffset;
        maxLength = pPayload->plHdr.payloadMetadataLength;
    }
    else if ( pRequest->requestType == kUARPDataRequestTypePayloadPayload )
    {
        pPayload = &(pAsset->payload);

        pRequest->payloadTag = pAsset->payload.plHdr.payloadTag;

        startOffset = pPayload->plHdr.payloadOffset;
        maxLength = pPayload->plHdr.payloadLength;
    }
    else
    {
        startOffset = 0;
        maxLength = 0;
    }
    
    /* validate the request  */
    __UARP_Require_Action( ( maxLength > 0 ), exit, status = kUARPStatusInvalidDataRequestLength );
    __UARP_Require_Action( ( pRequest->bytesRequested <= maxLength ), exit,
                          status = kUARPStatusInvalidDataRequestLength );
    
    tmp32 = pRequest->relativeOffset + pRequest->bytesRequested;
    __UARP_Require_Action( ( tmp32 <= maxLength ), exit, status = kUARPStatusInvalidDataRequestOffset );

    /* make the call */
    pRequest->absoluteOffset = startOffset + pRequest->relativeOffset;
    pRequest->currentOffset = pRequest->absoluteOffset + pRequest->bytesResponded;
    pRequest->bytesRemaining = pRequest->bytesRequested - pRequest->bytesResponded;

    status = uarpPlatformAssetRequestDataContinue( pAccessory, pAsset->pController, pAsset );

exit:
    return status;
}

/* MARK: CALLBACKS */

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformRequestTransmitMsgBuffer( void *pAccessoryDelegate, void *pControllerDelegate,
                                              uint8_t **ppBuffer, uint32_t *pLength )
{
    uint32_t status;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;

    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    status = pAccessory->callbacks.fRequestTransmitMsgBuffer( pAccessory->pDelegate, pController->pDelegate,
                                                             ppBuffer, pLength );

    return status;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformReturnTransmitMsgBuffer( void *pAccessoryDelegate, void *pControllerDelegate, uint8_t *pBuffer )
{
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;

    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    pAccessory->callbacks.fReturnTransmitMsgBuffer( pAccessory->pDelegate, pController->pDelegate, pBuffer );

    return;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformSendMessage( void *pAccessoryDelegate, void *pControllerDelegate,
                                 uint8_t *pBuffer, uint32_t length )
{
    uint32_t status;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    
    /* alias delegates */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;
    
    uarpLogDebug( kUARPLoggingCategoryPlatform, "SEND %u bytes to Remote UARP Controller %d",
                 length, pController->_controller.remoteControllerID );

    status = pAccessory->callbacks.fSendMessage( pAccessory->pDelegate, pController->pDelegate, pBuffer, length );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformDataTransferPause( void *pAccessoryDelegate, void *pControllerDelegate )
{
    uint32_t status;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    
    /* alias delegates */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Transfers PAUSED from Remote Controller %u",
                pController->_controller.remoteControllerID );

    status = pAccessory->callbacks.fDataTransferPause( pAccessory->pDelegate, pController->pDelegate );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformDataTransferResume( void *pAccessoryDelegate, void *pControllerDelegate )
{
    uint32_t status;
    struct uarpPlatformAsset *pAsset;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    
    /* alias delegates */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Transfers RESUMED from Remote Controller %u",
                pController->_controller.remoteControllerID );

    /* look through the assets related to this controller and see if there are any outstanding data requests
        if there are, it was likely dropped by the controller so we need to re-request  */
    for ( pAsset = pAccessory->pAssetList; pAsset != NULL; pAsset = pAsset->pNext )
    {
        if ( pAsset->pController != pController )
        {
            continue;
        }

        status = uarpPlatformAssetRequestDataContinue( pAccessory, pController, pAsset );
        __UARP_Check( status == kUARPStatusSuccess );
    }

    status = pAccessory->callbacks.fDataTransferResume( pAccessory->pDelegate, pController->pDelegate );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformDataRequestComplete( struct uarpPlatformAccessory *pAccessory,
                                         struct uarpPlatformAsset *pAsset,
                                         uint8_t reqType, uint32_t payloadTag,
                                         uint32_t offset, uint8_t *pBuffer, uint32_t length )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryPlatform, "Data Request Complete; %u bytes at offset %u", length, offset );

    if ( reqType == kUARPDataRequestTypeSuperBinaryHeader )
    {
        pAccessory->callbacks.fAssetReady( pAccessory->pDelegate, pAsset->pDelegate );

        status = kUARPStatusSuccess;
    }
    else if ( reqType == kUARPDataRequestTypeSuperBinaryMetadata )
    {
        status = uarpPlatformUpdateSuperBinaryMetaData( pAccessory, pAsset, pBuffer, length );
    }
    else if ( reqType == kUARPDataRequestTypeSuperBinaryPayloadHeader )
    {
        pAccessory->callbacks.fPayloadReady( pAccessory->pDelegate, pAsset->pDelegate );

        status = kUARPStatusSuccess;
    }
    else if ( reqType == kUARPDataRequestTypePayloadMetadata )
    {
        status = uarpPlatformUpdatePayloadMetaData( pAccessory, pAsset, pBuffer, length );
    }
    else if ( reqType == kUARPDataRequestTypePayloadPayload )
    {
        status = uarpPlatformUpdatePayloadPayload( pAccessory, pAsset, pBuffer, length );
    }
    else
    {
        status = kUARPStatusInvalidDataRequestType;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformQueryAccessoryInfo( void *pAccessoryDelegate, uint32_t infoType, void *pBuffer,
                                        uint32_t lengthBuffer, uint32_t *pLengthNeeded )
{
    uint32_t status;
    struct UARPVersion *pVersion;
    struct UARPLastErrorAction *pLastAction;
    struct uarpPlatformAccessory *pAccessory;

    /* alias delegates */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;

    *pLengthNeeded = lengthBuffer;
    
    switch ( infoType )
    {
        case kUARPTLVAccessoryInformationManufacturerName:

            status = pAccessory->callbacks.fManufacturerName( pAccessory->pDelegate, pBuffer, pLengthNeeded );

            break;
            
        case kUARPTLVAccessoryInformationModelName:
            
            status = pAccessory->callbacks.fModelName( pAccessory->pDelegate, pBuffer, pLengthNeeded );

            break;
            
        case kUARPTLVAccessoryInformationHardwareVersion:
            
            status = pAccessory->callbacks.fHardwareVersion( pAccessory->pDelegate, pBuffer, pLengthNeeded );

            break;
            
        case kUARPTLVAccessoryInformationSerialNumber:
            
            status = pAccessory->callbacks.fSerialNumber( pAccessory->pDelegate, pBuffer, pLengthNeeded );

            break;
            
        case kUARPTLVAccessoryInformationFirmwareVersion:
        case kUARPTLVAccessoryInformationStagedFirmwareVersion:

            *pLengthNeeded = sizeof( struct UARPVersion );
            if ( *pLengthNeeded > lengthBuffer )
            {
                status = kUARPStatusNoResources;
                break;
            }

            pVersion = (struct UARPVersion *)pBuffer;
            
            if ( infoType == kUARPTLVAccessoryInformationFirmwareVersion )
            {
                status = pAccessory->callbacks.fActiveFirmwareVersion( pAccessory->pDelegate, 0, pVersion );
            }
            else
            {
                status = pAccessory->callbacks.fStagedFirmwareVersion( pAccessory->pDelegate, 0, pVersion );
            }
            
            pVersion->major = uarpHtonl( pVersion->major );
            pVersion->minor = uarpHtonl( pVersion->minor );
            pVersion->release = uarpHtonl( pVersion->release );
            pVersion->build = uarpHtonl( pVersion->build );

            break;

        case kUARPTLVAccessoryInformationLastError:
            
            *pLengthNeeded = (uint32_t)sizeof( struct UARPLastErrorAction );
            if ( *pLengthNeeded > lengthBuffer )
            {
                status = kUARPStatusNoResources;
                break;
            }

            pLastAction = (struct UARPLastErrorAction *)pBuffer;
            
            status = pAccessory->callbacks.fLastError( pAccessory->pDelegate, pLastAction );
            
            pLastAction->lastAction = uarpHtonl( pLastAction->lastAction );
            pLastAction->lastError = uarpHtonl( pLastAction->lastError );
            
            status = kUARPStatusSuccess;
            break;
            
        default:
            *pLengthNeeded = 0;
            status = kUARPStatusUnknownInformationOption;
            break;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpPlatformApplyStagedAssets( void *pAccessoryDelegate, void *pControllerDelegate, uint16_t *pFlags )
{
    uint32_t status;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;
    
    /* alias delegates */
    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;

    status = pAccessory->callbacks.fApplyStagedAssets( pAccessory->pDelegate, pController->pDelegate, pFlags );

    return status;
}

/* -------------------------------------------------------------------------------- */
/* This routine is here in case we add a callback to the upper layer */
void uarpPlatformAssetRelease( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset )
{
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Asset Released" );

    return;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformAssetOrphan( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset )
{
    uarpLogInfo( kUARPLoggingCategoryPlatform, "Orphan %s Asset <%u>",
                uarpAssetIsSuperBinary( &pAsset->core ) ? "SuperBinary" : "Dynamic ",
                pAsset->core.assetID );

    pAsset->pController = NULL;

    pAccessory->callbacks.fAssetOrphaned( pAccessory->pDelegate, pAsset->pDelegate );
    
    return;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformAssetRescinded( void *pAccessoryDelegate, void *pControllerDelegate, uint16_t assetID )
{
    struct uarpPlatformAsset *pAsset;
    struct uarpPlatformAccessory *pAccessory;
    struct uarpPlatformController *pController;

    pAccessory = (struct uarpPlatformAccessory *)pAccessoryDelegate;
    pController = (struct uarpPlatformController *)pControllerDelegate;
    
    if ( assetID == kUARPAssetIDAllAssets )
    {
        for ( pAsset = pAccessory->pAssetList; pAsset; pAsset = pAsset->pNext  )
        {
            if ( pAsset->pController == pController )
            {
                uarpPlatformAssetRescind( pAccessory, pController, pAsset );
            }
        }
    }
    else
    {
        pAsset = uarpPlatformAssetFindByAssetID( pAccessory, pController, assetID );
        __UARP_Require( pAsset, exit );
        
        uarpPlatformAssetRescind( pAccessory, pController, pAsset );
    }

exit:
    return;
}

/* -------------------------------------------------------------------------------- */

void uarpPlatformAssetRescind( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformController *pController,
                              struct uarpPlatformAsset *pAsset )
{
    uarpLogDebug( kUARPLoggingCategoryPlatform, "Asset Rescinded from UARP Controller %d <Asset ID %u>",
                 pController->_controller.remoteControllerID, pAsset->core.assetID );
    
    pAsset->dataReq.requestType &= ~kUARPDataRequestTypeOutstanding;
    
    pAsset->internalFlags |= kUARPAssetMarkForCleanup;
    
    pAccessory->callbacks.fAssetRescinded( pAccessory->pDelegate, pController->pDelegate, pAsset->pDelegate );
}

/* -------------------------------------------------------------------------------- */

UARPBool uarpPlatformAssetIsCookieValid( struct uarpPlatformAccessory *pAccessory, struct uarpPlatformAsset *pAsset,
                                        struct uarpPlatformAssetCookie *pCookie )
{
    UARPBool isValid;
    UARPVersionComparisonResult versionResult;
    
    if ( pCookie == NULL )
    {
        isValid = kUARPNo;
    }
    else if ( pCookie->assetTag != pAsset->core.assetTag )
    {
        isValid = kUARPNo;
    }
    else if ( pCookie->assetTotalLength != pAsset->core.assetTotalLength )
    {
        isValid = kUARPNo;
    }
    else if ( pCookie->assetNumPayloads != pAsset->core.assetNumPayloads )
    {
        isValid = kUARPNo;
    }
    else if ( pCookie->selectedPayloadIndex >= pAsset->core.assetNumPayloads )
    {
        isValid = kUARPNo;
    }
    else
    {
        versionResult = uarpVersionCompare( &(pCookie->assetVersion), &(pAsset->core.assetVersion) );
        
        if ( versionResult != kUARPVersionComparisonResultIsEqual )
        {
            isValid = kUARPNo;
        }
        else
        {
            isValid = kUARPYes;
        }
    }
    
    return isValid;
}
