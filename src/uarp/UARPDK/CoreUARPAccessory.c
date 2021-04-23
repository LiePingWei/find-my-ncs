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

#include "CoreUARPPlatform.h"

#include "CoreUARPAccessory.h"
#include "CoreUARPUtils.h"


/* MARK: INTERNAL PROTOTYPES */


static uint32_t uarpAccessoryProcessVersionDiscoveryRequest( struct uarpAccessoryObj *pAccessory,
                                                     struct uarpRemoteControllerObj *pRemoteController,
                                                     struct UARPMsgVersionDiscoveryRequest *pRxMsg,
                                                     uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessAssetAvailableNotification( struct uarpAccessoryObj *pAccessory,
                                                        struct uarpRemoteControllerObj *pRemoteController,
                                                        struct UARPMsgAssetAvailableNotification *pRxMsg,
                                                        uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessAssetRescindedNotification( struct uarpAccessoryObj *pAccessory,
                                                               struct uarpRemoteControllerObj *pRemoteController,
                                                               struct UARPMsgAssetRescindedNotification *pRxMsg,
                                                               uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessAssetDataResponse( struct uarpAccessoryObj *pAccessory,
                                                      struct uarpRemoteControllerObj *pRemoteController,
                                                      struct UARPMsgAssetDataResponse *pRxMsg,
                                                      uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessAssetTransferNotification( struct uarpAccessoryObj *pAccessory,
                                                       struct uarpRemoteControllerObj *pRemoteController,
                                                       struct UARPMsgAssetDataTransferNotification *pRxMsg,
                                                       uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessApplyStagedAssetsRequest( struct uarpAccessoryObj *pAccessory,
                                                      struct uarpRemoteControllerObj *pRemoteController,
                                                      struct UARPMsgApplyStagedAssetsRequest *pRxMsg,
                                                      uint32_t lengthRxMsg );

static uint32_t uarpAccessoryProcessInformationRequest( struct uarpAccessoryObj *pAccessory,
                                                       struct uarpRemoteControllerObj *pRemoteController,
                                                       struct UARPMsgAccessoryInformationRequest *pRxMsg,
                                                       uint32_t lengthRxMsg );

#if !(UARP_DISABLE_VENDOR_SPECIFIC)

static uint32_t uarpAccessoryProcessVendorSpecific( struct uarpAccessoryObj *pAccessory,
                                                   struct uarpRemoteControllerObj *pRemoteController,
                                                   struct UARPMsgVendorSpecific *pRxMsg,
                                                   uint32_t lengthRxMsg );

#endif

static struct uarpRemoteControllerObj *
    uarpAccessoryRemoteControllerFind( struct uarpAccessoryObj *pAccessory,
                                      void *pDelegate );

static uint32_t uarpAccessoryTxAssetDataRequest( struct uarpAccessoryObj *pAccessory,
                                                struct uarpRemoteControllerObj *pRemoteController,
                                                uint16_t assetID, uint32_t dataOffset, uint32_t dataLength );

static uint32_t uarpAccessoryTxAssetProcessingNotification( struct uarpAccessoryObj *pAccessory,
                                                    struct uarpRemoteControllerObj *pRemoteController,
                                                    uint16_t assetID, uint16_t assetProcessingFlags );

static uint32_t uarpAccessoryTxSync( struct uarpAccessoryObj *pAccessory,
                                    struct uarpRemoteControllerObj *pRemoteController );

static uint32_t uarpAccessoryCallbackUpdateInformationTLV( struct uarpAccessoryObj *pAccessory,
                                                          struct uarpRemoteControllerObj *pRemoteController,
                                                          struct UARPTLVHeader *pTlv );

static uint32_t uarpAccessoryAllocAndPrepareTransmitBuffer( struct uarpAccessoryObj *pAccessory,
                                                           struct uarpRemoteControllerObj *pRemoteController,
                                                           uint8_t **ppBuffer, uint32_t *pBufferLength,
                                                           uint16_t msgType, uint16_t txMsgLength );

static uint32_t uarpAccessoryTransmitBuffer( struct uarpAccessoryObj *pAccessory,
                                            struct uarpRemoteControllerObj *pRemoteController,
                                            uint8_t *pBuffer, uint32_t bufferLength );

static uint32_t uarpAccessoryAssetProcessingComplete( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate,
                                                     uint16_t assetID, uint16_t assetProcessingFlags );

/* MARK: CORE  */

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryInit( struct uarpAccessoryObj *pAccessory,
                           struct uarpAccessoryCallbacksObj *pCallbacks,
                           void *pAccessoryDelegate )
{
    uint32_t status;
    
    /* qualifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusUnknownAccessory );
    __UARP_Verify_Action( pCallbacks, exit, status = kUARPStatusInvalidObject );
    __UARP_Verify_Action( pAccessoryDelegate, exit, status = kUARPStatusInvalidObject );

    memset( pAccessory, 0, sizeof( struct uarpAccessoryObj ) );

    pAccessory->callbacks = *pCallbacks;
    
    __UARP_Verify_Action( pAccessory->callbacks.fRequestTransmitMsgBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fReturnTransmitMsgBuffer, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fSendMessage, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fAccessoryQueryAccessoryInfo, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fAccessoryAssetOffered, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fAssetRescinded, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fAccessoryAssetDataResponse, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fUpdateDataTransferPause, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fUpdateDataTransferResume, exit, status = kUARPStatusInvalidFunctionPointer );
    __UARP_Verify_Action( pAccessory->callbacks.fApplyStagedAssets, exit, status = kUARPStatusInvalidFunctionPointer );

    pAccessory->pDelegate = pAccessoryDelegate;
    
    pAccessory->nextRemoteControllerID = 1;

    pAccessory->pControllerList = NULL;

    uarpLogDebug( kUARPLoggingCategoryAccessory, "Initialized New Accessory" );
    
    /* TODO: add capabilities field....
        - support re-connect / resume of superbinaries */

    /* done */
    status = kUARPStatusSuccess;

__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryRemoteControllerAdd( struct uarpAccessoryObj *pAccessory,
                                          struct uarpRemoteControllerObj *pRemoteController,
                                          void *pControllerDelegate )
{
    uint32_t status;
    struct uarpRemoteControllerObj *pTmp;
    
    /* qualifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pRemoteController, exit, status = kUARPStatusInvalidArgument );
    __UARP_Verify_Action( pControllerDelegate, exit, status = kUARPStatusInvalidArgument );

    /* ensure that we don't already have this controller on the list */
    pTmp = uarpAccessoryRemoteControllerFind( pAccessory, pControllerDelegate );
    __UARP_Require_Action( ( pTmp == NULL ), exit, status = kUARPStatusDuplicateController );
    
    /* init the new remote controller */
    memset( (void *)pRemoteController, 0, sizeof( struct uarpRemoteControllerObj ) );

    pRemoteController->remoteControllerID = pAccessory->nextRemoteControllerID++;
    
    pRemoteController->dataTransferAllowed = kUARPYes;
    
    pRemoteController->txMsgID = kUARPInitialTxMsgID;
    pRemoteController->lastRxMsgID = kUARPInitialTxMsgID - 1;

    pRemoteController->selectedProtocolVersion = kUARPProtocolVersionMinimum;
    
    pRemoteController->pDelegate = pControllerDelegate;

    /* update list */
    pRemoteController->pNext = pAccessory->pControllerList;
    
    pAccessory->pControllerList = pRemoteController;
    
    uarpLogInfo( kUARPLoggingCategoryAccessory, "Remote Controller Added" );

    status = uarpAccessoryTxSync( pAccessory, pRemoteController );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryRemoteControllerRemove( struct uarpAccessoryObj *pAccessory,
                                             struct uarpRemoteControllerObj *pRemoteController )
{
    uint32_t status;
    struct uarpRemoteControllerObj *pTmp;
    struct uarpRemoteControllerObj *pRemoteControllerList;

    /* Verifiers */
    __UARP_Verify_Action( pAccessory, exit, status = kUARPStatusUnknownAccessory );
    __UARP_Verify_Action( pRemoteController, exit, status = kUARPStatusInvalidArgument );

    /* remove and cleanup the matching remote controller */
    pRemoteControllerList = pAccessory->pControllerList;
    pAccessory->pControllerList = NULL;
    
    while ( pRemoteControllerList )
    {
        pTmp = pRemoteControllerList;
        
        pRemoteControllerList = pRemoteControllerList->pNext;
        
        if ( pTmp != pRemoteController )
        {
            pTmp->pNext = pAccessory->pControllerList;
            pAccessory->pControllerList = pTmp;
            continue;
        }
    }
    
    uarpLogInfo( kUARPLoggingCategoryAccessory, "Remote Controller Removed" );

    /* done */
    status = kUARPStatusSuccess;
    
__UARP_Verify_exit /* This resolves "exit:" if you have chosen to compile in __UARP_Verify_Action */
    return status;
}

/* -------------------------------------------------------------------------------- */

struct uarpRemoteControllerObj * uarpAccessoryRemoteControllerFind( struct uarpAccessoryObj *pAccessory, void *pDelegate )
{
    struct uarpRemoteControllerObj *pTmp;
    struct uarpRemoteControllerObj *pRemoteController;

    pRemoteController = NULL;

    if ( pAccessory && pDelegate )
    {
        for ( pTmp = pAccessory->pControllerList; pTmp; pTmp = pTmp->pNext  )
        {
            if ( pTmp->pDelegate == pDelegate )
            {
                pRemoteController = pTmp;
                break;
            }
        }
    }

    return pRemoteController;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryRecvMessage( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate,
                                  uint8_t *pRxMsg, uint32_t lengthRxMsg )
{
    uint32_t status;
    uint32_t lengthValidation;
    struct UARPMsgHeader *uarpMsg;
    struct uarpRemoteControllerObj *pRemoteController;

    /* look for the remote accessory in the list */
    pRemoteController = uarpAccessoryRemoteControllerFind( pAccessory, pControllerDelegate );
    
    if ( pRemoteController == NULL )
    {
        status = kUARPStatusUnknownController;
    }
    else if ( pRxMsg == NULL )
    {
        status = kUARPStatusInvalidMessage;
    }
    else if ( sizeof( struct UARPMsgHeader ) > lengthRxMsg  )
    {
        status = kUARPStatusInvalidMessageLength;
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    __UARP_Require_Quiet( status == kUARPStatusSuccess, exit );

    /* determine the type of message and get it to the proper dispatcher */
    uarpMsg = (struct UARPMsgHeader *)pRxMsg;
    
    /* ensure proper endianess */
    uarpMsg->msgType = uarpNtohs( uarpMsg->msgType );
    uarpMsg->msgPayloadLength = uarpNtohs( uarpMsg->msgPayloadLength );
    uarpMsg->msgID = uarpNtohs( uarpMsg->msgID );

    uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Type <0x%04x> ID <%u>", uarpMsg->msgType, uarpMsg->msgID );

    /* if this is the sync message, we will resync msg id */
    if ( uarpMsg->msgType == kUARPMsgSync )
    {
        status = kUARPStatusSuccess;
    }
    /* look for missing, duplicate, out of order packets */
    else if ( (uint16_t)( uarpMsg->msgID - 1 ) == pRemoteController->lastRxMsgID )
    {
        status = kUARPStatusSuccess;
    }
    else if ( uarpMsg->msgID == pRemoteController->lastRxMsgID )
    {
        uarpLogDebug( kUARPLoggingCategoryAccessory, "RX BAD MSG ID - duplicate <%u> last id <%u>",
                     uarpMsg->msgID, pRemoteController->lastRxMsgID );
        
        /* don't process a duplicate packet */
        status = kUARPStatusDuplicateMessageID;
        
        pRemoteController->uarpStats.packetsDuplicate++;
    }
    else if ( uarpMsg->msgID < pRemoteController->lastRxMsgID )
    {
        uarpLogDebug( kUARPLoggingCategoryAccessory, "RX BAD MSG ID - out of order <%u> last id <%u>",
                     uarpMsg->msgID, pRemoteController->lastRxMsgID );
        
        /* don't process an out of order packet */
        status = kUARPStatusOutOfOrderMessageID;
        
        pRemoteController->uarpStats.packetsOutOfOrder++;
    }
    else if ( uarpMsg->msgID > ( pRemoteController->lastRxMsgID + 1 ) )
    {
        uarpLogDebug( kUARPLoggingCategoryAccessory, "RX BAD MSG ID - missed <%u> last id <%u>",
                     uarpMsg->msgID, pRemoteController->lastRxMsgID );
        
        /* we should proceed if we missed a packet. */
        /* TODO: should we be tracking outstanding requests so we can resend ? */
        status = kUARPStatusSuccess;
        
        pRemoteController->uarpStats.packetsMissed++;
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    pRemoteController->lastRxMsgID = uarpMsg->msgID;
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* validate the length of the message */
    lengthValidation = uarpMsg->msgPayloadLength;
    lengthValidation += (uint32_t)sizeof( struct UARPMsgHeader );
    __UARP_Require_Action( ( lengthRxMsg == lengthValidation ), exit, status = kUARPStatusInvalidMessage );

    /* process the message */
    switch ( uarpMsg->msgType )
    {
        case kUARPMsgSync:

            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Sync" );

            break;
            
        case kUARPMsgVersionDiscoveryRequest:

            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Version Discovery Request" );

            status = uarpAccessoryProcessVersionDiscoveryRequest( pAccessory, pRemoteController,
                                                                 (struct UARPMsgVersionDiscoveryRequest *)pRxMsg,
                                                                 lengthRxMsg );
            break;
            
        case kUARPMsgAssetAvailableNotification:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Asset Available Notification" );

            status = uarpAccessoryProcessAssetAvailableNotification( pAccessory, pRemoteController,
                                                                    (struct UARPMsgAssetAvailableNotification *)pRxMsg,
                                                                    lengthRxMsg );
            break;
            
        case kUARPMsgAssetRescindedNotification:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Asset Rescinded Notification" );

            status = uarpAccessoryProcessAssetRescindedNotification( pAccessory, pRemoteController,
                                                                    (struct UARPMsgAssetRescindedNotification *)pRxMsg,
                                                                    lengthRxMsg );
            break;
            
        case kUARPMsgAssetDataResponse:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Asset Data Response" );

            status = uarpAccessoryProcessAssetDataResponse( pAccessory, pRemoteController,
                                                           (struct UARPMsgAssetDataResponse *)pRxMsg,
                                                           lengthRxMsg );
            break;
            
        case kUARPMsgAssetDataTransferNotification:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Data Transfer Notification Request" );

            status = uarpAccessoryProcessAssetTransferNotification( pAccessory, pRemoteController,
                                                                   (struct UARPMsgAssetDataTransferNotification *)pRxMsg,
                                                                   lengthRxMsg );
            break;
            
        case kUARPMsgApplyStagedAssetsRequest:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Apply Staged Asset Request" );

            status = uarpAccessoryProcessApplyStagedAssetsRequest( pAccessory, pRemoteController,
                                                           (struct UARPMsgApplyStagedAssetsRequest *)pRxMsg,
                                                           lengthRxMsg );
            break;
            
        case kUARPMsgAccessoryInformationRequest:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Accessory Information Request" );

            status = uarpAccessoryProcessInformationRequest( pAccessory, pRemoteController,
                                                            (struct UARPMsgAccessoryInformationRequest *)pRxMsg,
                                                            lengthRxMsg );
            break;
           
            
        case kUARPMsgAssetProcessingNotificationAck:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Asset Processing Notification Ack" );

            status = kUARPStatusSuccess;
            
            break;
           
#if !(UARP_DISABLE_CONTROLLER)
        /*
            kUARPMsgVersionDiscoveryResponse
            kUARPMsgAccessoryInformationResponse
            kUARPMsgAssetDataRequest
            kUARPMsgAssetProcessingNotification
            kUARPMsgApplyStagedAssetsResponse
            kUARPMsgAssetAvailableNotificationAck
            kUARPMsgAssetDataTransferNotificationAck
            kUARPMsgAssetRescindedNotificationAck
         */
#endif
            
#if !(UARP_DISABLE_VENDOR_SPECIFIC)

        case kUARPMsgVendorSpecific:
             
             uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Vendor Specific" );

             status = uarpAccessoryProcessVendorSpecific( pAccessory, pRemoteController,
                                                         (struct UARPMsgVendorSpecific *)pRxMsg,
                                                         lengthRxMsg );
             break;
#endif
            
        default:
            
            uarpLogDebug( kUARPLoggingCategoryAccessory, "MSG RCV Unknown 0x%8x", uarpMsg->msgType );

            status = kUARPStatusUnknownMessageType;

            break;
    }
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessVersionDiscoveryRequest( struct uarpAccessoryObj *pAccessory,
                                                     struct uarpRemoteControllerObj *pRemoteController,
                                                     struct UARPMsgVersionDiscoveryRequest *pRxMsg,
                                                     uint32_t lengthRxMsg )
{
    uint32_t status;
    uint32_t txMsgLength;
    uint16_t protocolVersionController;
    struct UARPMsgVersionDiscoveryResponse *pTxMsg;

    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgVersionDiscoveryRequest ) ), exit,
                     status = kUARPStatusInvalidMessage );

    /* Endian convert message, beyond header.  The calling function has converted the header already  */
    protocolVersionController = uarpNtohs( pRxMsg->protocolVersionController );
    
    /* make sure we can work with what the controller is offering */
    uarpLogDebug( kUARPLoggingCategoryAccessory, "MsgRx Version Discovery Request" );
    uarpLogDebug( kUARPLoggingCategoryAccessory, " - Accessory Max Protocol Version %u", pRemoteController->selectedProtocolVersion );
    uarpLogDebug( kUARPLoggingCategoryAccessory, " - Controller Max Protocol Version %u", protocolVersionController );

    if ( pRemoteController->selectedProtocolVersion > protocolVersionController )
    {
        pRemoteController->selectedProtocolVersion = protocolVersionController;
    }

    uarpLogDebug( kUARPLoggingCategoryAccessory, " - Selected Protocol Version %u", pRemoteController->selectedProtocolVersion );

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgVersionDiscoveryResponse );
    uarpLogDebug( kUARPLoggingCategoryAccessory, "Msg Tx lebgth %u", txMsgLength );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgVersionDiscoveryResponse,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Prepare our response */
    pTxMsg->status = uarpHtons( kUARPStatusSuccess );
    pTxMsg->protocolVersionAccessory = uarpHtons( pRemoteController->selectedProtocolVersion );
     
    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessAssetAvailableNotification( struct uarpAccessoryObj *pAccessory,
                                                        struct uarpRemoteControllerObj *pRemoteController,
                                                        struct UARPMsgAssetAvailableNotification *pRxMsg,
                                                        uint32_t lengthRxMsg )
{
    uint32_t status;
    uint32_t txMsgLength;
    struct uarpAssetCoreObj assetCore;
    struct UARPMsgAssetAvailableNotification *pTxMsg;
    
    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgAssetAvailableNotification ) ), exit,
                     status = kUARPStatusInvalidMessage );

    /* prepare the asset to begin the transfer */
    assetCore.assetTag = pRxMsg->assetTag; /* already in the right endian */
    assetCore.assetFlags = uarpNtohs( pRxMsg->assetFlags );
    assetCore.assetID = uarpNtohs( pRxMsg->assetID );
    
    uarpVersionEndianSwap( &(pRxMsg->assetVersion), &(assetCore.assetVersion) );

    assetCore.assetTotalLength = uarpNtohl( pRxMsg->assetLength );
    assetCore.assetNumPayloads = uarpNtohs( pRxMsg->assetNumPayloads );

    /* ACK this; note we did not swap RX Msg in place */
    txMsgLength = sizeof( struct UARPMsgAssetAvailableNotification );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgAssetAvailableNotificationAck,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    pTxMsg->assetTag = pRxMsg->assetTag;
    pTxMsg->assetFlags = pRxMsg->assetFlags;
    pTxMsg->assetID = pRxMsg->assetID;
    pTxMsg->assetVersion = pRxMsg->assetVersion;
    pTxMsg->assetLength = pRxMsg->assetLength;
    pTxMsg->assetNumPayloads = pRxMsg->assetNumPayloads;

    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* inform the FWUPAPP this is happening */
    status = pAccessory->callbacks.fAccessoryAssetOffered( pAccessory->pDelegate, pRemoteController->pDelegate,
                                                          &assetCore );
        
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessAssetRescindedNotification( struct uarpAccessoryObj *pAccessory,
                                                        struct uarpRemoteControllerObj *pRemoteController,
                                                        struct UARPMsgAssetRescindedNotification *pRxMsg,
                                                        uint32_t lengthRxMsg )
{
    uint16_t assetID;
    uint32_t status;
    uint32_t txMsgLength;
    struct UARPMsgAssetRescindedNotification *pTxMsg;
    
    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgAssetRescindedNotification ) ), exit,
                     status = kUARPStatusInvalidMessage );

    /* translate the asset ID */
    assetID = uarpNtohs( pRxMsg->assetID );

    uarpLogDebug( kUARPLoggingCategoryAccessory, "Asset Rescinded ID <%u> from Controller <%d>",
                 assetID, pRemoteController->remoteControllerID  );

    /* ACK this; note we did not swap RX Msg in place */
    txMsgLength = sizeof( struct UARPMsgAssetRescindedNotification );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgAssetRescindedNotificationAck,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    pTxMsg->assetID = pRxMsg->assetID;

    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* inform the FWUPAPP this is happening */
    pAccessory->callbacks.fAssetRescinded( pAccessory->pDelegate, pRemoteController->pDelegate, assetID );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessAssetDataResponse( struct uarpAccessoryObj *pAccessory,
                                               struct uarpRemoteControllerObj *pRemoteController,
                                               struct UARPMsgAssetDataResponse *pRxMsg,
                                               uint32_t lengthRxMsg )
{
    void *pBuffer;
    uint32_t status;
    uint16_t assetID;
    uint32_t lengthBuffer;
    uint16_t currentRequestStatus;
    uint32_t currentRequestedLength;
    uint32_t currentRespondedOffset;
    uint32_t currentRespondedLength;

    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgAssetDataResponse ) ), exit,
                     status = kUARPStatusInvalidMessage );

    /* look for the asset in our list and do some endian translation */
    assetID = uarpNtohs( pRxMsg->assetID );

    currentRequestStatus = uarpNtohs( pRxMsg->status );
    currentRespondedOffset = uarpNtohl( pRxMsg->dataOffset );
    currentRequestedLength = (uint32_t)uarpNtohs( pRxMsg->numBytesRequested );
    currentRespondedLength = (uint32_t)uarpNtohs( pRxMsg->numBytesResponded );

    uarpLogInfo( kUARPLoggingCategoryAccessory, "RCV %u bytes from offset %u of asset id %u",
                currentRespondedLength, currentRespondedOffset, assetID );

    /* look for any obvious errors */
    if ( currentRequestStatus != kUARPStatusSuccess )
    {
        status = currentRequestStatus;
    }
    else if ( currentRespondedLength > ( lengthRxMsg - sizeof( struct UARPMsgAssetDataResponse ) ) )
    {
        status = kUARPStatusInvalidDataResponseLength;
    }
    else if ( currentRespondedLength > currentRequestedLength )
    {
        status = kUARPStatusInvalidDataResponseLength;
    }
    else
    {
        status = kUARPStatusSuccess;
    }
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* alias the buffer */
    pBuffer = (void *)( (uint8_t *)pRxMsg + sizeof( struct UARPMsgAssetDataResponse ) );
    lengthBuffer = currentRespondedLength;

    /* inform the upper layer */
    status = pAccessory->callbacks.fAccessoryAssetDataResponse( pAccessory->pDelegate, pRemoteController->pDelegate,
                                                               assetID, pBuffer, lengthBuffer, currentRespondedOffset );
        
exit:
    /* TODO: if failure for any reason, tell FWUPAPP we need to purge this asset */

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessAssetTransferNotification( struct uarpAccessoryObj *pAccessory,
                                                       struct uarpRemoteControllerObj *pRemoteController,
                                                       struct UARPMsgAssetDataTransferNotification *pRxMsg,
                                                       uint32_t lengthRxMsg )
{
    uint32_t status;
    uint16_t assetTransferFlags;
    uint32_t txMsgLength;
    struct UARPMsgAssetDataTransferNotification *pTxMsg;

    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgAssetDataTransferNotification ) ), exit,
                          status = kUARPStatusInvalidMessage );

    /* setup the assetID in proper endianess */
    assetTransferFlags = uarpNtohs( pRxMsg->assetTransferFlags );
    
    /* ACK this; note we did not swap RX Msg in place */
    txMsgLength = sizeof( struct UARPMsgAssetDataTransferNotification );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgAssetDataTransferNotificationAck,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    pTxMsg->assetTransferFlags = pRxMsg->assetTransferFlags;

    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* adhere to kUARPAssetDataTransferPause / kUARPAssetDataTransferResume */
    if ( ( assetTransferFlags & kUARPAssetDataTransferPause ) && ( pRemoteController->dataTransferAllowed == kUARPYes ) )
    {
        uarpLogInfo( kUARPLoggingCategoryAccessory, "Data Transfer Paused by Controller" );

        pRemoteController->dataTransferAllowed = kUARPNo;
        
        status = pAccessory->callbacks.fUpdateDataTransferPause( pAccessory->pDelegate, pRemoteController->pDelegate );
    }
    else if ( ( assetTransferFlags & kUARPAssetDataTransferResume ) && ( pRemoteController->dataTransferAllowed == kUARPNo ) )
    {
        uarpLogInfo( kUARPLoggingCategoryAccessory, "Data Transfer Resumed by Controller" );

        pRemoteController->dataTransferAllowed = kUARPYes;

        status = pAccessory->callbacks.fUpdateDataTransferResume( pAccessory->pDelegate, pRemoteController->pDelegate );
    }
    else
    {
        status = kUARPStatusInvalidDataTransferNotification;
    }
        
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessApplyStagedAssetsRequest( struct uarpAccessoryObj *pAccessory,
                                                      struct uarpRemoteControllerObj *pRemoteController,
                                                      struct UARPMsgApplyStagedAssetsRequest *pRxMsg,
                                                      uint32_t lengthRxMsg )
{
    uint32_t status;
    uint16_t flags;
    uint32_t txMsgLength;
    struct UARPMsgApplyStagedAssetsResponse *pTxMsg;
    
    /* tell the FWUPAPP */
    status = pAccessory->callbacks.fApplyStagedAssets( pAccessory->pDelegate, pRemoteController->pDelegate, &flags );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgApplyStagedAssetsResponse );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgApplyStagedAssetsResponse,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Prepare our response */
    pTxMsg->status = uarpHtons( kUARPStatusSuccess );
    pTxMsg->flags = uarpHtons( flags );
    
    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryTxAssetProcessingNotification( struct uarpAccessoryObj *pAccessory,
                                                    struct uarpRemoteControllerObj *pRemoteController,
                                                    uint16_t assetID, uint16_t assetProcessingFlags )
{
    uint32_t status;
    uint32_t txMsgLength;
    struct UARPMsgAssetProcessingNotification *pTxMsg;

    /* We could actually get here with a NULL pRemoteController, for abandoning an asset
        which was left behind by a, now unreachable, remote controller */

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgAssetProcessingNotification );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgAssetProcessingNotification,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Prepare our response */
    pTxMsg->assetID = uarpHtons( assetID );
    pTxMsg->assetProcessingFlags = uarpHtons( assetProcessingFlags );
    
    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
        
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryTxAssetDataRequest( struct uarpAccessoryObj *pAccessory,
                                         struct uarpRemoteControllerObj *pRemoteController,
                                         uint16_t assetID, uint32_t dataOffset, uint32_t dataLength )
{
    uint32_t status;
    uint32_t txMsgLength;
    uint32_t numBytesRequested;
    struct UARPMsgAssetDataRequest *pTxMsg;

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgAssetDataRequest );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgAssetDataRequest,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Prepare our response */
    pTxMsg->assetID = uarpHtons( assetID );
    pTxMsg->dataOffset = uarpHtonl( dataOffset );

    /* determine how big of a chunk we can download */
    numBytesRequested = dataLength;

    if ( numBytesRequested > kUARPMaxBytesRequested )
    {
        numBytesRequested = kUARPMaxBytesRequested;
    }

    pTxMsg->numBytesRequested = uarpHtons( (uint16_t)numBytesRequested );

    uarpLogInfo( kUARPLoggingCategoryAccessory, "REQ %u bytes (instead of %u) from offset %u of asset id %u",
                numBytesRequested, dataLength, dataOffset, assetID );

    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
        
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryTxSync( struct uarpAccessoryObj *pAccessory, struct uarpRemoteControllerObj *pRemoteController )
{
    uint32_t status;
    uint32_t txMsgLength;
    struct UARPMsgHeader *pTxMsg;

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgHeader );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgSync,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );



    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
        
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryProcessInformationRequest( struct uarpAccessoryObj *pAccessory,
                                                struct uarpRemoteControllerObj *pRemoteController,
                                                struct UARPMsgAccessoryInformationRequest *pRxMsg,
                                                uint32_t lengthRxMsg )
{
    uint32_t status;
    uint32_t txMsgLength;
    uint32_t bufferLength;
    uint32_t lengthTlv;
    struct UARPTLVHeader *pTlv;
    struct UARPMsgAccessoryInformationResponse *pTxMsg;

    /* Verifiers */
    __UARP_Require_Action( ( lengthRxMsg >= sizeof( struct UARPMsgAccessoryInformationRequest ) ), exit,
                          status = kUARPStatusInvalidMessage );

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgAccessoryInformationResponse ) + sizeof( struct UARPTLVHeader );

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, &bufferLength,
                                                        kUARPMsgAccessoryInformationResponse,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Query the TLV from the product, and have it copy the response inline if there is room */
    pTlv = (struct UARPTLVHeader *)((uint8_t *)pTxMsg + sizeof( struct UARPMsgAccessoryInformationResponse ));
    lengthTlv = bufferLength - txMsgLength;

    pTlv->tlvType = uarpNtohl( pRxMsg->informationOption );
    pTlv->tlvLength = lengthTlv;

    /* Prepare our response, need to update the header's msgPayloadLength */
    status = uarpAccessoryCallbackUpdateInformationTLV( pAccessory, pRemoteController, pTlv );

    if ( status == kUARPStatusSuccess )
    {
        txMsgLength += pTlv->tlvLength;
        bufferLength = (uint16_t)txMsgLength - (uint16_t)sizeof( struct UARPMsgHeader );

        pTxMsg->msgHdr.msgPayloadLength = uarpHtons( bufferLength );

        pTlv->tlvType = uarpHtonl( pTlv->tlvType );
        pTlv->tlvLength = uarpHtonl( pTlv->tlvLength );
    }
    
    pTxMsg->status = uarpHtons( status );

    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryCallbackUpdateInformationTLV( struct uarpAccessoryObj *pAccessory,
                                                   struct uarpRemoteControllerObj *pRemoteController,
                                                   struct UARPTLVHeader *pTlv )
{
    void *pBuffer;
    uint32_t lengthBuffer;
    uint32_t status;
    
    pBuffer = (void *)( (uint8_t *)pTlv + sizeof( struct UARPTLVHeader ) );
    lengthBuffer = pTlv->tlvLength;
    
    /* call the callback, unless this is something we track internally */
    if ( pTlv->tlvType == kUARPTLVAccessoryInformationStatistics )
    {
        struct UARPStatistics *pStats;
        
        pStats = (struct UARPStatistics *)pBuffer;
        
        if ( lengthBuffer >= sizeof( struct UARPStatistics ) )
        {
            pStats->packetsNoVersionAgreement = uarpHtonl( pRemoteController->uarpStats.packetsNoVersionAgreement );
            pStats->packetsMissed = uarpHtonl( pRemoteController->uarpStats.packetsMissed );
            pStats->packetsDuplicate = uarpHtonl( pRemoteController->uarpStats.packetsDuplicate );
            pStats->packetsOutOfOrder = uarpHtonl( pRemoteController->uarpStats.packetsOutOfOrder );

            lengthBuffer = sizeof( struct UARPStatistics );
            
            status = kUARPStatusSuccess;
        }
        else
        {
            status = kUARPStatusNoResources;
        }
    }
    else
    {
        status = pAccessory->callbacks.fAccessoryQueryAccessoryInfo( pAccessory->pDelegate, pTlv->tlvType,
                                                                    pBuffer, lengthBuffer, &lengthBuffer );
    }
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* copy back the length */
    pTlv->tlvLength = lengthBuffer;
    
    /* done */
    status = kUARPStatusSuccess;

exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetDeny( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate, uint16_t assetID )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryAccessory, "Asset <%d> Deny", assetID );

    status = uarpAccessoryAssetProcessingComplete( pAccessory, pControllerDelegate,
                                                  assetID, kUARPAssetProcessingFlagsUploadDenied );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetAbandon( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate, uint16_t assetID )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryAccessory, "Asset <%d> Abandon", assetID );

    status = uarpAccessoryAssetProcessingComplete( pAccessory, pControllerDelegate,
                                                  assetID, kUARPAssetProcessingFlagsUploadAbandoned );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetCorrupt( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate, uint16_t assetID )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryAccessory, "Asset <%d> Corrupt", assetID );

    status = uarpAccessoryAssetProcessingComplete( pAccessory, pControllerDelegate,
                                                  assetID, kUARPAssetProcessingFlagsAssetCorrupt );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetStaged( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate, uint16_t assetID )
{
    uint32_t status;

    uarpLogInfo( kUARPLoggingCategoryAccessory, "Asset <%d> Staged", assetID );

    status = uarpAccessoryAssetProcessingComplete( pAccessory, pControllerDelegate,
                                                  assetID, kUARPAssetProcessingFlagsUploadComplete );

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetProcessingComplete( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate,
                                              uint16_t assetID, uint16_t assetProcessingFlags )
{
    uint32_t status;
    struct uarpRemoteControllerObj *pRemoteController;

    /* alias the remote controller */
    pRemoteController = uarpAccessoryRemoteControllerFind( pAccessory, pControllerDelegate );

    if ( pRemoteController )
    {
        status = uarpAccessoryTxAssetProcessingNotification( pAccessory, pRemoteController,
                                                            assetID, assetProcessingFlags );
    }
    else if ( assetProcessingFlags == kUARPAssetProcessingFlagsUploadAbandoned )
    {
        /* remote controller can be NULL when abandoning an asset */
        status = kUARPStatusSuccess;
    }
    else
    {
        status = kUARPStatusUnknownController;
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAssetRequestData( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate,
                                       uint16_t assetID, uint32_t requestOffset, uint32_t requestLength )
{
    uint32_t status;
    struct uarpRemoteControllerObj *pRemoteController;

    /* alias the remote controller */
    pRemoteController = uarpAccessoryRemoteControllerFind( pAccessory, pControllerDelegate );
    
    if ( pRemoteController == NULL )
    {
        status = kUARPStatusUnknownController;
    }
    else if ( pRemoteController->dataTransferAllowed == kUARPNo )
    {
        status = kUARPStatusDataTransferPaused;
    }
    else
    {
        status = uarpAccessoryTxAssetDataRequest( pAccessory, pRemoteController, assetID, requestOffset, requestLength );
    }

    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryAllocAndPrepareTransmitBuffer( struct uarpAccessoryObj *pAccessory,
                                                    struct uarpRemoteControllerObj *pRemoteController,
                                                    uint8_t **ppBuffer,  uint32_t *pBufferLength,
                                                    uint16_t msgType, uint16_t txMsgLength )
{
    uint32_t status;
    uint32_t bufferLength;
    struct UARPMsgHeader *pTxMsg;
   
    /* just in case fRequestTransmitMsgBuffer doesn't do proper initialization */
    *ppBuffer = NULL;

    if ( pBufferLength )
    {
        *pBufferLength = 0;
    }

    status = pAccessory->callbacks.fRequestTransmitMsgBuffer( pAccessory->pDelegate,
                                                             pRemoteController->pDelegate,
                                                             (uint8_t **)ppBuffer, &bufferLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );
    __UARP_Require( ( bufferLength >= txMsgLength ), exit );

    /* Prepare our response */
    pTxMsg = (struct UARPMsgHeader *)(*ppBuffer);
    
    pTxMsg->msgType = uarpHtons( msgType );

    pTxMsg->msgPayloadLength = uarpHtons( txMsgLength - (uint16_t)sizeof( struct UARPMsgHeader ) );

    if ( pBufferLength )
    {
        *pBufferLength = bufferLength;
    }
    
    status = kUARPStatusSuccess;
    
exit:
    return status;
}

/* -------------------------------------------------------------------------------- */

uint32_t uarpAccessoryTransmitBuffer( struct uarpAccessoryObj *pAccessory,
                                     struct uarpRemoteControllerObj *pRemoteController,
                                     uint8_t *pBuffer, uint32_t bufferLength )
{
    uint32_t status;
    struct UARPMsgHeader *pMsgHdr;
    
    /* update the msgID field here, not at alloc time */
    pMsgHdr = (struct UARPMsgHeader *)pBuffer;
    
    pMsgHdr->msgID = uarpHtons( pRemoteController->txMsgID );

    status = pAccessory->callbacks.fSendMessage( pAccessory->pDelegate,
                                                pRemoteController->pDelegate,
                                                pBuffer, bufferLength );
    
    if ( status == kUARPStatusSuccess )
    {
        pRemoteController->txMsgID++;
    }
    else
    {
        pAccessory->callbacks.fReturnTransmitMsgBuffer( pAccessory->pDelegate,
                                                       pRemoteController->pDelegate,
                                                       pBuffer );
    }
    
    return status;
}

/* -------------------------------------------------------------------------------- */

#if !(UARP_DISABLE_VENDOR_SPECIFIC)

uint32_t uarpAccessoryProcessVendorSpecific( struct uarpAccessoryObj *pAccessory,
                                            struct uarpRemoteControllerObj *pRemoteController,
                                            struct UARPMsgVendorSpecific *pRxMsg,
                                            uint32_t lengthRxMsg )
{
    uint8_t *pBuffer;
    uint32_t length;
    uint32_t status;
    uint32_t lengthNeeded;

    /* Verifiers  */
    lengthNeeded = sizeof( struct UARPMsgVendorSpecific );
    __UARP_Require_Action( ( lengthRxMsg >= lengthNeeded ), exit, status = kUARPStatusInvalidMessage );

    /* Tell FWUPAPP about this option */
    pBuffer = (uint8_t *)( (uint8_t *)pRxMsg + sizeof( struct UARPMsgVendorSpecific ) );

    uarpLogDebug( kUARPLoggingCategoryAccessory, "Vendor Specific Message from Remote UARP Controller %d "
                 "OUI <%02x-%02x-%02x> MsgType <0x%04x>",
                 pRemoteController->remoteControllerID, pRxMsg->oui[0], pRxMsg->oui[1], pRxMsg->oui[2],
                 uarpNtohs( pRxMsg->msgType ) );

    /* TODO: we should be looking at msg payload length too */
    if ( pAccessory->callbacks.fVendorSpecific )
    {
        length = lengthRxMsg - sizeof( struct UARPMsgVendorSpecific );
        
        status = pAccessory->callbacks.fVendorSpecific( pAccessory->pDelegate, pRemoteController->pDelegate,
                                                        pRxMsg->oui, uarpNtohs( pRxMsg->msgType ),
                                                        pBuffer, length );
    }
    else
    {
        status = kUARPStatusUnknownMessageType;
    }
    
exit:
    return status;
}

#endif

/* -------------------------------------------------------------------------------- */

#if !(UARP_DISABLE_VENDOR_SPECIFIC)

uint32_t uarpAccessoryTxMsgVendorSpecific( struct uarpAccessoryObj *pAccessory, void *pControllerDelegate,
                                          uint8_t oui[kUARPOUILength], uint16_t msgType,
                                          uint8_t *pBuffer, uint32_t lengthBuffer )
{
    uint8_t *pTmp;
    uint32_t status;
    uint32_t txMsgLength;
    struct UARPMsgVendorSpecific *pTxMsg;
    struct uarpRemoteControllerObj *pRemoteController;

    /* alias objects */
    pRemoteController = uarpAccessoryRemoteControllerFind( pAccessory, pControllerDelegate );
    __UARP_Require_Action( pRemoteController, exit, status = kUARPStatusUnknownController );

    /* get a buffer */
    txMsgLength = sizeof( struct UARPMsgVendorSpecific ) + lengthBuffer;

    status = uarpAccessoryAllocAndPrepareTransmitBuffer( pAccessory->pDelegate,
                                                        pRemoteController->pDelegate,
                                                        (uint8_t **)&pTxMsg, NULL,
                                                        kUARPMsgVendorSpecific,
                                                        txMsgLength );
    __UARP_Require( ( status == kUARPStatusSuccess ), exit );

    /* Prepare our response */
    pTxMsg->oui[0] = oui[0];
    pTxMsg->oui[1] = oui[1];
    pTxMsg->oui[2] = oui[2];
    pTxMsg->msgType = uarpHtons( msgType );
    
    if ( pBuffer && ( lengthBuffer > 0 ) )
    {
        pTmp = (uint8_t *)pTxMsg + (uint32_t)sizeof( struct UARPMsgVendorSpecific );
        memcpy( pTmp, pBuffer, lengthBuffer );
    }
    
    /* send the message */
    status = uarpAccessoryTransmitBuffer( pAccessory->pDelegate, pRemoteController->pDelegate,
                                         (uint8_t *)pTxMsg, txMsgLength );

exit:
    return status;
}

#endif

