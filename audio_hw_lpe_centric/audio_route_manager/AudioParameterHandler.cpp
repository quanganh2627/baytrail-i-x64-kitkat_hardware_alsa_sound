#include <utils/Log.h>
#include <utils/String8.h>
#include <utils/Errors.h>
#include <media/AudioParameter.h>

#include "AudioParameterHandler.h"

namespace android_audio_legacy
{

static const char kFilePath[] = "/mnt/asec/media/audio_param.dat";
static const int kReadBufSize = 500;

CAudioParameterHandler::CAudioParameterHandler()
{
    restore();
}

status_t CAudioParameterHandler::saveParameters(const String8& keyValuePairs)
{
    add(keyValuePairs);

    if (save() != NO_ERROR) {

        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

void CAudioParameterHandler::add(const String8& keyValuePairs)
{
    AudioParameter newParameters(keyValuePairs);
    uint32_t uiParameter;

    for (uiParameter = 0; uiParameter < newParameters.size(); uiParameter++) {

        String8 key, value;

        // Retrieve new parameter
        newParameters.getAt(uiParameter, key, value);

        // Add / merge it with stored ones
        mAudioParameter.add(key, value);
    }
}

status_t CAudioParameterHandler::save()
{
    FILE *fp = fopen(kFilePath, "w+");
    if(!fp){

        return UNKNOWN_ERROR;
    }

    String8 param = mAudioParameter.toString();

    fwrite(param.string(), sizeof(char), param.length(), fp);

    fclose(fp);
    return NO_ERROR;
}

status_t CAudioParameterHandler::restore()
{
    FILE *fp = fopen(kFilePath, "r");
    if (!fp) {

        return UNKNOWN_ERROR;
    }
    char str[kReadBufSize];
    int readSize = fread(str, sizeof(char), kReadBufSize - 1, fp);

    if (readSize < 0) {

        fclose(fp);
        return UNKNOWN_ERROR;
    }
    // 0 terminate
    str[readSize] = '\0';
    fclose(fp);

    add(String8(str));
    return NO_ERROR;
}

String8 CAudioParameterHandler::getParameters() const
{
    return mAudioParameter.toString();
}
}
