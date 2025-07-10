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
