/* SyncSemaphore.h
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

#define MASK_32_BITS_MSB    0x7FFFFFFF
#define REMOVE_32_BITS_MSB(bitfield) bitfield & MASK_32_BITS_MSB

#define __UNUSED __attribute__((unused))

class CUtils
{
public:
    enum Direction {

        EInput = 0,
        EOutput,

        ENbDirections
    };
};
