/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2025 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#ifndef PLAY_BY_SAMPLE_INTERFACE
#define PLAY_BY_SAMPLE_INTERFACE

#include <alterlib/atypes.h>

/**
 * @file
 * @brief Audio playback interface (16-bit stereo, unsigned)
 */

namespace alt
{
    namespace audio
    {

        /**
         * @brief Initializes the audio system.
         * @param freq Sample rate in Hz (default: 44100).
         * @return True on success, false on failure.
         */
        bool init(int freq = 44100);

        /**
         * @brief Shuts down the audio system and releases resources.
         */
        void destroy();

        /**
         * @brief Pushes a 32-bit audio sample to the buffer.
         * @param val 32-bit unsigned audio sample.
         * @return False if the buffer is full, true otherwise.
         */
        bool push(uint32 val);

        /**
         * @brief Flushes the audio buffer.
         * @return True on success, false on failure.
         */
        bool flush();

        /**
         * @brief Checks if the audio system is ready for new data.
         * @return True if ready, false otherwise.
         */
        bool ready();

        /**
         * @brief Sets into pause state
         */
        void pause(bool pause);

        /**
         * @brief Sets the playback volume.
         * @param val Volume value (0.0 - 1.0).
         */
        void set_volume(float val);

        /**
         * @brief Swaps left and right audio channels.
         * @param val True to swap channels, false to keep default order.
         */
        void swap_channels(bool val);

        /**
         * @brief Returns the buffer utilization.
         * @return Utilization in percent (0-100).
         */
        int utilization();

    }
}
#endif
