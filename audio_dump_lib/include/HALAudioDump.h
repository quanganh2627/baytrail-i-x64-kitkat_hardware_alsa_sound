/* HALAudioDump.h
 **
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

#include <stdint.h>
#include <sys/types.h>
#include <string>

/**
 * Limit file size to about 20 MB for audio dump files
 * to avoid filling the mass storage to the brim
 */
static const uint32_t MAX_DUMP_FILE_SIZE = 20 * 1024 * 1024;

class CHALAudioDump
{
public:
    CHALAudioDump();
    ~CHALAudioDump();

    /**
     * Dumps the raw audio samples in a file. The name of the
     * audio file contains the infos on the dump characteristics.
     *
     * @param[in] buf const pointer the buffer to be dumped
     * @param[in] bytes size in bytes to be written
     * @param[in] isOutput  boolean for direction of the stream
     * @param[in] samplingRate sample rate of the stream
     * @param[in] channelNb number of channels in the sample spec
     * @param[in] nameContext context of the dump to be appended in the name of the dump file
     *
     **/
    void dumpAudioSamples(const void *buf,
                          ssize_t bytes,
                          bool isOutput,
                          uint32_t samplingRate, // Use SampleSpec class as soon as it
                                                 // is unbound to the route manager, to
                                                 // avoid circular dependencies
                          uint32_t channelNb,
                          const std::string &nameContext);

    void Close();

private:
    const char *streamDirectionStr(bool isOutput);

    /**
     * Writes the PCM samples in the dump file.
     * Checks that file size does not exceed its maximum size
     *
     * @param[in] buf const pointer to the buffer to be dumped
     * @param[in] bytes size in bytes to be written
     *
     * @return error code
     **/
    int32_t writeDumpFile(const void *buf,
                          ssize_t bytes);

    FILE *dumpFile;
    uint32_t fileCount;

    /**
     * Maximum number of files per dump instance
     */
    static const uint32_t MAX_NUMBER_OF_FILES;

    /**
     * Store stream directions in and out
     */
    static const char *streamDirections[];

    /**
     * Dump directory path to store audio dump files
     */
    static const char *dumpDirPath;
};
