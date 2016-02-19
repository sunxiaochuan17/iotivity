//******************************************************************
//
// Copyright 2015 Samsung Electronics All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/**
 * @file
 *
 * This file contains the implementation for EasySetup Enrollee device
 */

#include "easysetup.h"
#include "softap.h"
#include "onboarding.h"
#include "logger.h"
#include "resourcehandler.h"

/**
 * @var ES_ENROLLEE_TAG
 * @brief Logging tag for module name.
 */
#define ES_ENROLLEE_TAG "ES"

//-----------------------------------------------------------------------------
// Private variables
//-----------------------------------------------------------------------------

/**
 * @var gTargetSsid
 * @brief Target SSID of the Soft Access point to which the device has to connect
 */
static char gTargetSsid[MAXSSIDLEN];

/**
 * @var gTargetPass
 * @brief Password of the target access point to which the device has to connect
 */
static char gTargetPass[MAXNETCREDLEN];

/**
 * @var gEnrolleeStatusCb
 * @brief Fucntion pointer holding the callback for intimation of EasySetup Enrollee status callback
 */
static EventCallback gEnrolleeStatusCb = NULL;

/**
 * @var gIsSecured
 * @brief Variable to check if secure mode is enabled or not.
 */
static bool gIsSecured = false;

//-----------------------------------------------------------------------------
// Private internal function prototypes
//-----------------------------------------------------------------------------
void OnboardingCallback(ESResult esResult);
void ProvisioningCallback(ESResult esResult);
void OnboardingCallbackTargetNet(ESResult esResult);
static bool ValidateParam(OCConnectivityType networkType, const char *ssid, const char *passwd,
              EventCallback cb);


void OnboardingCallback(ESResult esResult)
{
        OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG, "OnboardingCallback with  result = %d", esResult);
        if(esResult == ES_OK)
        {
            gEnrolleeStatusCb(esResult, ES_ON_BOARDED_STATE);
        }
        else
        {
            OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG,
                        "Onboarding is failed callback result is = %d", esResult);
            gEnrolleeStatusCb(esResult, ES_INIT_STATE);
        }
}

void ProvisioningCallback(ESResult esResult)
{
    OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG, "ProvisioningCallback with  result = %d", esResult);

    if (esResult == ES_RECVTRIGGEROFPROVRES)
    {
        GetTargetNetworkInfoFromProvResource(gTargetSsid, gTargetPass);
        gEnrolleeStatusCb(ES_OK, ES_PROVISIONED_STATE);
        OIC_LOG(DEBUG, ES_ENROLLEE_TAG, "Connecting with target network");

        // Connecting/onboarding to target network
        ConnectToWiFiNetwork(gTargetSsid, gTargetPass, OnboardingCallbackTargetNet);
    }
    else
    {
       OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG, "Provisioning is failed callback result is = %d", esResult);
       // Resetting Enrollee to ONBOARDED_STATE as Enrollee is alreday onboarded in previous step
       gEnrolleeStatusCb(ES_OK, ES_ON_BOARDED_STATE);
    }
}

void OnboardingCallbackTargetNet(ESResult esResult)
{
    OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG, "OnboardingCallback on target network with result = %d",
                                                                                        esResult);
    if(esResult == ES_OK)
    {
        gEnrolleeStatusCb(esResult, ES_ON_BOARDED_TARGET_NETWORK_STATE);
    }
    else
    {
        OIC_LOG_V(DEBUG, ES_ENROLLEE_TAG,
                    "Onboarding is failed on target network and callback result is = %d", esResult);
        // Resetting Enrollee state to the ES_PROVISIONED_STATE
        // as device is already being provisioned with target network creds.
        gEnrolleeStatusCb(esResult, ES_PROVISIONED_STATE);
    }
}

ESResult InitEasySetup(OCConnectivityType networkType, const char *ssid, const char *passwd,
        bool isSecured,
        EventCallback cb)
{
    OIC_LOG(INFO, ES_ENROLLEE_TAG, "InitEasySetup IN");
    if(!ValidateParam(networkType,ssid,passwd,cb))
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG,
                            "InitEasySetup::Stopping Easy setup due to invalid parameters");
        return ES_ERROR;
    }

    //Init callback
    gEnrolleeStatusCb = cb;

    gIsSecured = isSecured;

    // TODO : This onboarding state has to be set by lower layer, as they better
    // knows when actually on-boarding started.
    cb(ES_ERROR,ES_ON_BOARDING_STATE);

    OIC_LOG(INFO, ES_ENROLLEE_TAG, "received callback");
    OIC_LOG(INFO, ES_ENROLLEE_TAG, "onboarding now..");

    if(!ESOnboard(ssid, passwd, OnboardingCallback))
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "InitEasySetup::On-boarding failed");
        cb(ES_ERROR, ES_INIT_STATE);
        return ES_ERROR;
    }

    OIC_LOG(INFO, ES_ENROLLEE_TAG, "InitEasySetup OUT");
    return ES_OK;
}

ESResult TerminateEasySetup()
{
    UnRegisterResourceEventCallBack();

    //Delete Prov resource
    if (DeleteProvisioningResource() != OC_STACK_OK)
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "Deleting prov resource error!!");
        return ES_ERROR;
    }

    //Delete Prov resource
#ifdef ESWIFI
    if (DeleteNetworkResource() != OC_STACK_OK)
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "Deleting prov resource error!!");
        return ES_ERROR;
    }
#endif

    OIC_LOG(ERROR, ES_ENROLLEE_TAG, "TerminateEasySetup success");
    return ES_OK;
}

ESResult InitProvisioning()
{
    OIC_LOG(INFO, ES_ENROLLEE_TAG, "InitProvisioning <<IN>>");

    if (CreateProvisioningResource(gIsSecured) != OC_STACK_OK)
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "CreateProvisioningResource error");
        return ES_ERROR;
    }

#ifdef ESWIFI
    if (CreateNetworkResource() != OC_STACK_OK)
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "CreateNetworkResource error");
        return ES_ERROR;
    }
#endif

    RegisterResourceEventCallBack(ProvisioningCallback);

    OIC_LOG(INFO, ES_ENROLLEE_TAG, "InitProvisioning OUT");
    return ES_RESOURCECREATED;
}

static bool ValidateParam(OCConnectivityType /*networkType*/, const char *ssid, const char *passwd,
              EventCallback cb)
{
    if (!ssid || !passwd || !cb)
    {
        OIC_LOG(ERROR, ES_ENROLLEE_TAG, "ValidateParam - Invalid parameters");
        return false;
    }
    return true;
}

