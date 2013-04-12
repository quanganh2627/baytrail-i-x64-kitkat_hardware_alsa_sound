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

#define LOG_TAG "RouteManager/StreamRoute"
#include <utils/Log.h>

#include <ALSAStreamOps.h>
#include "AudioPlatformHardware.h"
#include "AudioUtils.h"
#include <tinyalsa/asoundlib.h>
#include <hardware_legacy/power.h>

#include "AudioStreamRoute.h"

#define base    CAudioRoute

namespace android_audio_legacy
{

const char* const CAudioStreamRoute::POWER_LOCK_TAG[CUtils::ENbDirections] = {"AudioInLock","AudioOutLock"};

CAudioStreamRoute::CAudioStreamRoute(uint32_t uiRouteIndex,
                                     CAudioPlatformState *platformState) :
    CAudioRoute(uiRouteIndex, platformState),
    _pEffectSupported(0),
    _pcCardName(CAudioPlatformHardware::getRouteCardName(uiRouteIndex))
{
    for (int iDir = 0; iDir < CUtils::ENbDirections; iDir++) {

       _stStreams[iDir].pCurrent = NULL;
       _stStreams[iDir].pNew = NULL;
       _astPcmDevice[iDir] = NULL;
       _aiPcmDeviceId[iDir] = CAudioPlatformHardware::getRouteDeviceId(uiRouteIndex, iDir);
       _astPcmConfig[iDir] = CAudioPlatformHardware::getRoutePcmConfig(uiRouteIndex, iDir);
       _acPowerLockTag[iDir] = POWER_LOCK_TAG[iDir];

       _routeSampleSpec[iDir].setFormat(CAudioUtils::convertTinyToHalFormat(_astPcmConfig[iDir].format));
       _routeSampleSpec[iDir].setSampleRate(_astPcmConfig[iDir].rate);
       _routeSampleSpec[iDir].setChannelCount(_astPcmConfig[iDir].channels);
       _routeSampleSpec[iDir].setChannelsPolicy(CAudioPlatformHardware::getChannelsPolicy(uiRouteIndex, iDir));
    }
}

//
// Basic identical conditions fonction
// To be implemented by derivated classes if different
// route policy
//
bool CAudioStreamRoute::needReconfiguration(bool bIsOut) const
{
    // TBD: what conditions will lead to set the need reconfiguration flag for this route???
    // The route needs reconfiguration except if:
    //      - still used by the same stream
    //      - the stream is using the same device
    if (base::needReconfiguration(bIsOut) &&
            ((_stStreams[bIsOut].pCurrent != _stStreams[bIsOut].pNew) ||
             (_stStreams[bIsOut].pCurrent->getCurrentDevices() != _stStreams[bIsOut].pNew->getNewDevices()))) {

        return true;
    }
    return false;
}

status_t CAudioStreamRoute::route(bool bIsOut)
{
    status_t err = openPcmDevice(bIsOut);
    if (err != NO_ERROR) {

        // Failed to open PCM device -> bailing out
        return err;
    }

    return attachNewStream(bIsOut);
}

void CAudioStreamRoute::unroute(bool bIsOut)
{
    closePcmDevice(bIsOut);

    detachCurrentStream(bIsOut);
}

void CAudioStreamRoute::configure(bool bIsOut)
{
    // Same stream is attached to this route, consumme the new device
    if (_stStreams[bIsOut].pCurrent == _stStreams[bIsOut].pNew) {

        // Consume the new device(s)
        _stStreams[bIsOut].pCurrent->setCurrentDevices(_stStreams[bIsOut].pCurrent->getNewDevices());
    } else {

        // Route is still in use, but the stream attached to this route has changed...
        // Unroute previous stream
        detachCurrentStream(bIsOut);

        // route new stream
        attachNewStream(bIsOut);
    }
}

void CAudioStreamRoute::resetAvailability()
{
    for (int iDir = 0; iDir < CUtils::ENbDirections; iDir++) {

        if (_stStreams[iDir].pNew) {

            _stStreams[iDir].pNew->resetRoute();
            _stStreams[iDir].pNew = NULL;
        }
    }
    base::resetAvailability();
}

status_t CAudioStreamRoute::setStream(ALSAStreamOps* pStream)
{
    ALOGV("%s to %s route", __FUNCTION__, getName().c_str());
    bool bIsOut = pStream->isOut();

    assert(!_stStreams[bIsOut].pNew);

    _stStreams[bIsOut].pNew = pStream;

    _stStreams[bIsOut].pNew->setNewRoute(this);

    return NO_ERROR;
}

bool CAudioStreamRoute::isApplicable(uint32_t uiDevices, int iMode, bool bIsOut, uint32_t uiMask) const
{
    ALOGV("%s: is Route %s applicable? ",__FUNCTION__, getName().c_str());
    ALOGV("%s: \t\t\t bIsOut=%s && uiMask=0x%X & _uiApplicableMask[%s]=0x%X", __FUNCTION__,
          bIsOut? "output" : "input",
          uiMask,
          bIsOut ? "output" : "input",
          _applicabilityRules[bIsOut].uiMask);

    if ((uiMask & _applicabilityRules[bIsOut].uiMask) == 0) {

        return false;
    }
    // Base class does not have much work to do than checking
    // if no stream is already using it and if not condemened
    return base::isApplicable(uiDevices, iMode, bIsOut);
}

bool CAudioStreamRoute::available(bool bIsOut)
{
    // A route is available if no stream is already using it and if not condemened
    return !isBlocked() && !_stStreams[bIsOut].pNew;
}

bool CAudioStreamRoute::currentlyUsed(bool bIsOut) const
{
    return _stStreams[bIsOut].pCurrent != NULL;
}

bool CAudioStreamRoute::willBeUsed(bool bIsOut) const
{
    return _stStreams[bIsOut].pNew != NULL;
}

int CAudioStreamRoute::getPcmDeviceId(bool bIsOut) const
{
    return _aiPcmDeviceId[bIsOut];
}

pcm* CAudioStreamRoute::getPcmDevice(bool bIsOut) const
{
    LOG_ALWAYS_FATAL_IF(_astPcmDevice[bIsOut] == NULL);

    return _astPcmDevice[bIsOut];

}

const pcm_config& CAudioStreamRoute::getPcmConfig(bool bIsOut) const
{
    return _astPcmConfig[bIsOut];
}

const char* CAudioStreamRoute::getCardName() const
{
    return _pcCardName;
}

status_t CAudioStreamRoute::attachNewStream(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(_stStreams[bIsOut].pNew == NULL);

    status_t err = _stStreams[bIsOut].pNew->attachRoute();

    if (err != NO_ERROR) {

        // Failed to open output stream -> bailing out
        return err;
    }
    _stStreams[bIsOut].pCurrent = _stStreams[bIsOut].pNew;

    return NO_ERROR;
}

void CAudioStreamRoute::detachCurrentStream(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(_stStreams[bIsOut].pCurrent == NULL);

    _stStreams[bIsOut].pCurrent->detachRoute();
    _stStreams[bIsOut].pCurrent = NULL;
}

bool CAudioStreamRoute::isEffectSupported(const effect_uuid_t* uuid) const
{
    std::list<const effect_uuid_t*>::const_iterator it;
    it = std::find_if(_pEffectSupported.begin(), _pEffectSupported.end(),
                      std::bind2nd(hasEffect(), uuid));

    return it != _pEffectSupported.end();
}

status_t CAudioStreamRoute::openPcmDevice(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(_astPcmDevice[bIsOut] != NULL);

    acquirePowerLock(bIsOut);

    pcm_config config = getPcmConfig(bIsOut);
    ALOGD("%s called for card (%s,%d)",
                                __FUNCTION__,
                                getCardName(),
                                getPcmDeviceId(bIsOut));
    ALOGD("%s\t\t config=rate(%d), format(%d), channels(%d))",
                                __FUNCTION__,
                                config.rate,
                                config.format,
                                config.channels);
    ALOGD("%s\t\t period_size=%d, period_count=%d",
                                __FUNCTION__,
                                config.period_size,
                                config.period_count);
    ALOGD("%s\t\t startTh=%d, stop Th=%d silence Th=%d",
                                __FUNCTION__,
                                config.start_threshold,
                                config.stop_threshold,
                                config.silence_threshold);

    //
    // Opens the device in BLOCKING mode (default)
    // No need to check for NULL handle, tiny alsa
    // guarantee to return a pcm structure, even when failing to open
    // it will return a reference on a "bad pcm" structure
    //
    uint32_t uiFlags= (bIsOut ? PCM_OUT : PCM_IN);
    _astPcmDevice[bIsOut] = pcm_open(CAudioUtils::getCardNumberByName(getCardName()), getPcmDeviceId(bIsOut), uiFlags, &config);
    if (_astPcmDevice[bIsOut] && !pcm_is_ready(_astPcmDevice[bIsOut])) {

        ALOGE("%s: Cannot open tinyalsa (%s,%d) device for %s stream (error=%s)", __FUNCTION__,
              getCardName(),
              getPcmDeviceId(bIsOut),
              bIsOut? "output" : "input",
              pcm_get_error(_astPcmDevice[bIsOut]));
        pcm_close(_astPcmDevice[bIsOut]);
        _astPcmDevice[bIsOut] = NULL;
        releasePowerLock(bIsOut);
        return NO_MEMORY;
    }
    return NO_ERROR;
}

void CAudioStreamRoute::closePcmDevice(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(_astPcmDevice[bIsOut] == NULL);

    ALOGD("%s called for card (%s,%d)",
                                __FUNCTION__,
                                getCardName(),
                                getPcmDeviceId(bIsOut));
    pcm_close(_astPcmDevice[bIsOut]);
    _astPcmDevice[bIsOut] = NULL;

    releasePowerLock(bIsOut);
}

void CAudioStreamRoute::acquirePowerLock(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(_bPowerLock[bIsOut]);

    acquire_wake_lock(PARTIAL_WAKE_LOCK, _acPowerLockTag[bIsOut]);
    _bPowerLock[bIsOut] = true;
}

void CAudioStreamRoute::releasePowerLock(bool bIsOut)
{
    LOG_ALWAYS_FATAL_IF(!_bPowerLock[bIsOut]);

    release_wake_lock(_acPowerLockTag[bIsOut]);
    _bPowerLock[bIsOut] = false;
}
}       // namespace android
