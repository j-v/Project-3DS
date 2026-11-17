/*
    Copyright 2022-2024 Hydr8gon
    Copyright 2026 Project 3DS contributors
    Modified 2026 by Project 3DS contributors.

    This file is part of Project 3DS, a derivative work of Project DS
    <https://github.com/Hydr8gon/Project-DS>.

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_AUDIO_ENGINE_H
#define P3DS_AUDIO_ENGINE_H

#include <cstdint>
#include <string>

#include <3ds.h>

namespace p3ds {

/**
 * The sound of the game.
 *
 * The music is streamed straight from the OGG files of the data folder, so
 * no conversion step is needed and no intermediate files are written. The
 * decoding runs on its own thread, and the DSP does the resampling.
 *
 * The OGG files of the game are quadraphonic, while the DSP plays stereo, so
 * the channels are mixed down while they are decoded: the first and third
 * channels form the left one and the second and fourth form the right one.
 */
class AudioEngine
{
public:
    AudioEngine();

    // The engine owns the decode thread, the DSP channel and the linear
    // buffers, so a copy would free the same resources twice
    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;

    /** Sets the sound up. */
    bool init();

    /** Cleans the sound up. */
    void exit();

    /**
     * Decodes the first buffers of a song without playing it.
     *
     * The decoding takes a while, so it is done before the chart starts. The
     * sound itself then starts with start(), which only hands the buffers to
     * the DSP and returns at once. This keeps the first sample of the song
     * and the start of the chart on the same frame.
     *
     * @param path      the OGG file of the song
     * @param offsetMs  how much the song is moved, in milliseconds
     */
    bool prepare(const std::string &path, int offsetMs);

    /** Prepares a song and starts it at once. */
    bool play(const std::string &path, int offsetMs);

    /** Hands a prepared song to the DSP. */
    void start();

    /** Stops the song. */
    void stop();

    /** Pauses the song without losing its place. */
    void pause();

    /** Starts the song again where it was paused. */
    void resume();

    /** Whether a song is loaded, playing or paused. */
    bool isPlaying() const { return playing; }

    /** Whether the loaded song is paused. */
    bool isPaused() const { return paused; }

    /** How many samples of the song were decoded. */
    uint64_t samplesPlayed() const { return positionFrames; }

    /** How many frames the DSP dropped, which is how starved the decoder was. */
    uint32_t droppedFrames() const;

    /** How many frames the DSP played. */
    uint32_t frameCount() const;

private:
    static const int BufferCount = 4;
    static const int BufferMilliseconds = 120;
    static const int ThreadStackSize = 64 * 1024;

    /** The most channels a file may have before it is refused. */
    static const int MaxInputChannels = 8;

    /** The highest sample rate the buffers are sized for. */
    static const int MaxSampleRate = 96000;

    static void decodeThread(void *argument);
    static void soundFrameCallback(void *argument);

    bool fillBuffer(int index);
    void closeFileLocked();
    void stopLocked();
    void setupChannelLocked();

    struct OggVorbisFile;

    OggVorbisFile *file = nullptr;
    Thread thread = nullptr;
    LightLock lock;
    bool lockReady = false;

    int16_t *buffer = nullptr;
    int16_t *scratch = nullptr;
    ndspWaveBuf waveBufs[BufferCount];

    uint32_t bufferCapacityFrames = 0;
    uint32_t framesPerBuffer = 0;
    uint32_t sampleRate = 44100;
    int inputChannels = 2;

    uint64_t positionFrames = 0;
    uint64_t lagFrames = 0;
    uint64_t silenceFrames = 0;
    int preparedBuffers = 0;

    bool playing = false;
    bool paused = false;

#ifdef P3DS_DIAG
    /** How long the decoding of the buffers took, to spot starvation. */
    uint64_t fillTicks = 0;
    uint64_t maxFillTicks = 0;
    uint32_t fillCalls = 0;

    /**
     * The time between two finished buffers, which is the rate at which the
     * DSP consumed them. It should be one buffer duration, and twice that
     * would mean that the DSP is silent between buffers.
     */
    uint64_t lastFillTick = 0;
    uint64_t intervalTicks = 0;
    uint64_t maxIntervalTicks = 0;
    uint32_t intervalCalls = 0;
    uint64_t startTick = 0;
#endif
};

} // namespace p3ds

#endif // P3DS_AUDIO_ENGINE_H
