/* tinyalsa_if.cpp
 **
 ** Copyright 2012 Intel Corporation
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

#define LOG_TAG "TinyAlsaModule"
#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <AudioHardwareALSACommon.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <signal.h>

#include <tinyalsa/asoundlib.h>


#define MAX_RETRY (6)
#define USEC_PER_SEC                    ((int)1000000)

namespace android_audio_legacy
{

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t s_init(alsa_device_t *);
static status_t s_open(alsa_handle_t *handle, const char* cardName, int deviceId, const pcm_config& pcmConfig);
static status_t s_init_stream(alsa_handle_t *handle, bool isOut, const pcm_config& config);
static status_t s_stop(alsa_handle_t *handle);
static status_t s_standby(alsa_handle_t *);
static status_t s_close(alsa_handle_t *);
static status_t s_volume(alsa_handle_t *, uint32_t, float);
static int get_card_number_by_name(const char* name);

static hw_module_methods_t s_module_methods = {
    open : s_device_open
};

extern "C" hw_module_t HAL_MODULE_INFO_SYM;

hw_module_t HAL_MODULE_INFO_SYM =
{
    tag           : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id            : TINYALSA_HARDWARE_MODULE_ID,
    name          : "tinyalsa wrapper module",
    author        : "Intel Corporation",
    methods       : &s_module_methods,
    dso           : 0,
    reserved      : { 0, },
};

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

// ----------------------------------------------------------------------------

static const pcm_config pcm_config_default = {
   channels             : 0,
   rate                 : 0,
   period_size          : 0,
   period_count         : 0,
   format               : PCM_FORMAT_S16_LE,
   start_threshold      : 0 ,
   stop_threshold       : 0 ,
   silence_threshold    : 0,
   avail_min            : 0,
};

static alsa_handle_t _defaultsOut = {
    module             : 0,
    handle             : NULL,
    config             : pcm_config_default,
    latencyInUs        : 0,
    flags              : PCM_OUT,
    modPrivate         : 0,
};

static alsa_handle_t _defaultsIn = {
    module             : 0,
    handle             : NULL,
    config             : pcm_config_default,
    latencyInUs          : 0,
    flags              : PCM_IN,
    modPrivate         : 0,
};

// ----------------------------------------------------------------------------

static status_t s_init(alsa_device_t *module)
{
    _defaultsOut.module = module;
    _defaultsIn.module = module;

    return NO_ERROR;
}

static status_t s_init_stream(alsa_handle_t *handle, bool isOut, const pcm_config& config)
{
    LOGD("%s called for %s stream", __FUNCTION__, isOut? "output" : "input");

    if (isOut) {

        *handle = _defaultsOut;

    } else {

        *handle = _defaultsIn;
    }

    handle->config = config;
    handle->latencyInUs = (int64_t)config.period_size * config.period_count * USEC_PER_SEC / config.rate;

    return NO_ERROR;
}

static int s_device_open(const hw_module_t* module, const char* name,
                         hw_device_t** device)
{
    (void)name;
    alsa_device_t *dev;
    dev = (alsa_device_t *) malloc(sizeof(*dev));
    if (!dev) {

        return -ENOMEM;
    }

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = s_init;
    dev->open = s_open;
    dev->standby = s_standby;
    dev->close = s_close;
    dev->volume = s_volume;
    dev->initStream = s_init_stream;
    dev->stop = s_stop;

    *device = &dev->common;
    return 0;
}

static status_t s_open(alsa_handle_t *handle,
                       const char* cardName,
                       int deviceId,
                       const pcm_config& pcmConfig)
{
    handle->config = pcmConfig;

    ALOGD("%s called for card (%s,%d)",
                                __FUNCTION__,
                                cardName,
                                deviceId);
    ALOGD("%s\t\t config=rate(%d), format(%d), channels(%d))",
                                __FUNCTION__,
                                handle->config.rate,
                                handle->config.format,
                                handle->config.channels);
    ALOGD("%s\t\t period_size=%d, period_count=%d",
                                __FUNCTION__,
                                handle->config.period_size,
                                handle->config.period_count);
    ALOGD("%s\t\t startTh=%d, stop Th=%d silence Th=%d",
                                __FUNCTION__,
                                handle->config.start_threshold,
                                handle->config.stop_threshold,
                                handle->config.silence_threshold);

    //
    // Opens the device in BLOCKING mode (default)
    // No need to check for NULL handle, tiny alsa
    // guarantee to return a pcm structure, even when failing to open
    // it will return a reference on a "bad pcm" structure
    //
    handle->handle = pcm_open(get_card_number_by_name(cardName), deviceId, handle->flags, &handle->config);
    if (handle->handle && !pcm_is_ready(handle->handle)) {

        ALOGE("cannot open pcm device driver: %s", pcm_get_error(handle->handle));
        pcm_close(handle->handle);
        return NO_MEMORY;
    }

    return NO_ERROR;
}

static status_t s_stop(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    if (h) {

        LOGD("%s stopping stream \n", __func__);
        err = pcm_stop(h);
    }

    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_standby(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    if (h) {
        err = pcm_close(h);
        handle->handle = NULL;
    }

    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_close(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    handle->handle = NULL;

    if (h) {

        err = pcm_close(h);
    }

    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_volume(alsa_handle_t *handle, uint32_t devices, float volume)
{
    (void)handle;
    (void)devices;
    (void)volume;
    return NO_ERROR;
}

// This function return the card number associated with the card ID (name)
// passed as argument
static int get_card_number_by_name(const char* name)

{
    char id_filepath[PATH_MAX] = {0};
    char number_filepath[PATH_MAX] = {0};
    ssize_t written;

    snprintf(id_filepath, sizeof(id_filepath), "/proc/asound/%s", name);

    written = readlink(id_filepath, number_filepath, sizeof(number_filepath));
    if (written < 0) {
        ALOGE("Sound card %s does not exist", name);
        return written;
    } else if (written >= (ssize_t)sizeof(id_filepath)) {
        // This will probably never happen
        return -ENAMETOOLONG;
    }

    // We are assured, because of the check in the previous elseif, that this
    // buffer is null-terminated.  So this call is safe.
    // 4 == strlen("card")
    return atoi(number_filepath + 4);
}

}; // namespace android_audio_legacy
