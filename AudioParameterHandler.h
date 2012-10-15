#include <utils/Errors.h>
#include <utils/String8.h>
#include <media/AudioParameter.h>

namespace android {
    class AudioParameterHandler
    {
        public:
            AudioParameterHandler();
            status_t saveParameters(const String8& keyValuePairs); //Backup the parameters
            String8 getParameters() const; //Return the stored parameters from filesystem

        private:
            status_t save(); //Save the mAudioParameter into filesystem
            status_t restore(); //Read the parameters from filesystem and add into mAudioParameters
            void add(const String8& keyValuePairs); //Add the parameters into mAudioParameters

            mutable AudioParameter mAudioParameter; //All of parameters will be saved into this variable
    };
}
