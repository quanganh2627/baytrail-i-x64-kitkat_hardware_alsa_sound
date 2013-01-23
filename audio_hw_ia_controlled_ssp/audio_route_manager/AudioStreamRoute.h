/*
 ** Copyright 2013 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#pragma once

#include <tinyalsa/asoundlib.h>

#include "AudioRoute.h"
#include "SampleSpec.h"

namespace android_audio_legacy
{

class ALSAStreamOps;

class CAudioStreamRoute : public CAudioRoute
{
public:
    CAudioStreamRoute(uint32_t uiRouteIndex,
                      CAudioPlatformState* platformState);

    pcm* getPcmDevice(bool bIsOut) const;

    const CSampleSpec getSampleSpec(bool bIsOut) const { return _routeSampleSpec[bIsOut]; }

    virtual RouteType getRouteType() const { return CAudioRoute::EStreamRoute; }

    // Assign a new stream to this route
    status_t setStream(ALSAStreamOps* pStream);

    // Route order
    virtual status_t route(bool bForOutput);

    // Unroute order
    virtual void unroute(bool bForOutput);

    // Configure order
    virtual void configure(bool bIsOut);

    // Inherited for AudioRoute - called from RouteManager
    virtual void resetAvailability();

    // Inherited from AudioRoute - Called from AudioRouteManager
    virtual bool available(bool bIsOut);

    // Inherited from AudioRoute - Called from RouteManager
    virtual bool currentlyUsed(bool bIsOut) const;

    // Inherited from AudioRoute - Called from RouteManager
    virtual bool willBeUsed(bool bIsOut) const;

    // Inherited for AudioRoute - called from RouteManager
    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlagsInputSource = 0) const;

    // Filters the unroute/route
    virtual bool needReconfiguration(bool bIsOut) const;

    // Get amount of silence delay upon stream opening
    virtual uint32_t getOutputSilencePrologMs() const { return 0; }

protected:
    struct {

        ALSAStreamOps* pCurrent;
        ALSAStreamOps* pNew;
    } _stStreams[CUtils::ENbDirections];

private:
    int getPcmDeviceId(bool bIsOut) const;

    const pcm_config& getPcmConfig(bool bIsOut) const;

    const char* getCardName() const;

    android::status_t openPcmDevice(bool bIsOut);

    void closePcmDevice(bool bIsOut);

    android::status_t attachNewStream(bool bIsOut);

    void detachCurrentStream(bool bIsOut);

    void acquirePowerLock(bool bIsOut);

    void releasePowerLock(bool bIsOut);

    const char* _pcCardName;
    int _aiPcmDeviceId[CUtils::ENbDirections];
    pcm_config _astPcmConfig[CUtils::ENbDirections];

    pcm* _astPcmDevice[CUtils::ENbDirections];

    CSampleSpec             _routeSampleSpec[CUtils::ENbDirections];

    bool                    _bPowerLock[CUtils::ENbDirections];
    const char*             _acPowerLockTag[CUtils::ENbDirections];

    static const char* const POWER_LOCK_TAG[CUtils::ENbDirections];
};
};        // namespace android

