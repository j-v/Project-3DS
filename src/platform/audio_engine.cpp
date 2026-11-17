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

#include "audio_engine.h"

#include "platform.h"

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <cstdio>
#include <cstring>

namespace p3ds {

/** The file the sound is played from. */
struct AudioEngine::OggVorbisFile
{
    OggVorbis_File vorbis;
    FILE *handle = nullptr;
};

/** The event the decoding thread waits on. */
static LightEvent decodeEvent;

/** Whether the decoding thread is still running. */
static volatile bool decodeRunning = false;

static inline int16_t clampSample(int32_t sample)
{
    if (sample < -32768)
        return -32768;

    if (sample > 32767)
        return 32767;

    return (int16_t)sample;
}

/**
 * Mixes a frame of any channel count down to stereo.
 *
 * The even channels are the left one and the odd channels are the right one,
 * so a quadraphonic file becomes front-left plus back-left and front-right
 * plus back-right.
 */
static void downmix(int16_t *target, const int16_t *source, uint32_t frames, int channels)
{
    if (channels == 1)
    {
        for (uint32_t i = 0; i < frames; i++)
            target[i * 2] = target[i * 2 + 1] = source[i];

        return;
    }

    for (uint32_t i = 0; i < frames; i++)
    {
        int32_t left = 0;
        int32_t right = 0;

        for (int c = 0; c < channels; c++)
        {
            if (c & 1)
                right += source[i * channels + c];
            else
                left += source[i * channels + c];
        }

        target[i * 2] = clampSample(left);
        target[i * 2 + 1] = clampSample(right);
    }
}

AudioEngine::AudioEngine()
{
}

bool AudioEngine::init()
{
    if (R_FAILED(ndspInit()))
        return false;

    // The buffers are sized for the highest rate the files use, so a song of
    // any rate fits without reallocating them when it is selected
    bufferCapacityFrames = MaxSampleRate * BufferMilliseconds / 1000;
    uint32_t bufferSize = bufferCapacityFrames * 2 * sizeof(int16_t);

    buffer = (int16_t *)linearAlloc(bufferSize * BufferCount);
    scratch = (int16_t *)linearAlloc(bufferCapacityFrames * MaxInputChannels * sizeof(int16_t));

    if (!buffer || !scratch)
    {
        if (buffer)
            linearFree(buffer);

        if (scratch)
            linearFree(scratch);

        buffer = nullptr;
        scratch = nullptr;
        ndspExit();
        return false;
    }

    memset(buffer, 0, bufferSize * BufferCount);
    memset(scratch, 0, bufferCapacityFrames * MaxInputChannels * sizeof(int16_t));
    memset(waveBufs, 0, sizeof(waveBufs));

    for (int i = 0; i < BufferCount; i++)
    {
        waveBufs[i].data_vaddr = buffer + i * bufferCapacityFrames * 2;
        waveBufs[i].nsamples = 0;
        waveBufs[i].status = NDSP_WBUF_DONE;
    }

    LightLock_Init(&lock);
    lockReady = true;

    LightEvent_Init(&decodeEvent, RESET_ONESHOT);
    decodeRunning = true;

    // The decoding thread lives as long as the sound does, so starting a song
    // does not have to create it
    int32_t priority = 0x30;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    priority -= 1;
    priority = priority < 0x18 ? 0x18 : priority;
    priority = priority > 0x3F ? 0x3F : priority;

    thread = threadCreate(decodeThread, this, ThreadStackSize, priority, -1, false);

    if (!thread)
    {
        decodeRunning = false;
        lockReady = false;
        linearFree(buffer);
        linearFree(scratch);
        buffer = nullptr;
        scratch = nullptr;
        ndspExit();
        return false;
    }

    ndspSetCallback(soundFrameCallback, nullptr);
    return true;
}

void AudioEngine::exit()
{
    stop();

    decodeRunning = false;
    LightEvent_Signal(&decodeEvent);

    if (thread)
    {
        threadJoin(thread, UINT64_MAX);
        threadFree(thread);
        thread = nullptr;
    }

    lockReady = false;

    if (scratch)
    {
        linearFree(scratch);
        scratch = nullptr;
    }

    if (buffer)
    {
        linearFree(buffer);
        buffer = nullptr;
    }

    ndspExit();
}

void AudioEngine::closeFileLocked()
{
    if (!file)
        return;

    // ov_clear closes the FILE that ov_open was given, so it must not be
    // closed again here
    ov_clear(&file->vorbis);
    delete file;
    file = nullptr;
}

void AudioEngine::stopLocked()
{
#ifdef P3DS_DIAG
    if (fillCalls)
    {
        FILE *log = fopen("sdmc:/project-3ds-diag.log", "a");

        if (log)
        {
            double averageMs = (double)fillTicks / fillCalls * 1000.0 / (double)platform::TICK_RATE;
            double maximumMs = (double)maxFillTicks * 1000.0 / (double)platform::TICK_RATE;
            double averageIntervalMs = intervalCalls ?
                (double)intervalTicks / intervalCalls * 1000.0 / (double)platform::TICK_RATE : 0;
            double maximumIntervalMs = (double)maxIntervalTicks * 1000.0 / (double)platform::TICK_RATE;
            double playedMs = startTick ?
                (double)(svcGetSystemTick() - startTick) * 1000.0 / (double)platform::TICK_RATE : 0;
            fprintf(log, "audio: %u fills, average %.1f ms, longest %.1f ms, "
                    "refill every %.1f ms (max %.1f), played %.1f ms, %u dropped frames\n",
                    (unsigned)fillCalls, averageMs, maximumMs, averageIntervalMs,
                    maximumIntervalMs, playedMs, (unsigned)ndspGetDroppedFrames());
            fclose(log);
        }
    }

    fillTicks = 0;
    maxFillTicks = 0;
    fillCalls = 0;
    lastFillTick = 0;
    intervalTicks = 0;
    maxIntervalTicks = 0;
    intervalCalls = 0;
    startTick = 0;
#endif

    playing = false;
    paused = false;
    preparedBuffers = 0;
    ndspChnReset(0);
    closeFileLocked();
}

void AudioEngine::setupChannelLocked()
{
    // The DSP always plays stereo, no matter how many channels the file has
    ndspChnReset(0);
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, sampleRate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
}

bool AudioEngine::prepare(const std::string &path, int offsetMs)
{
    if (!lockReady)
        return false;

    LightLock_Lock(&lock);
    stopLocked();

    FILE *handle = fopen(path.c_str(), "rb");

    if (!handle)
    {
        LightLock_Unlock(&lock);
        return false;
    }

    OggVorbisFile *newFile = new OggVorbisFile();
    newFile->handle = handle;

    if (ov_open(handle, &newFile->vorbis, nullptr, 0) < 0)
    {
        fclose(handle);
        delete newFile;
        LightLock_Unlock(&lock);
        return false;
    }

    file = newFile;

    vorbis_info *info = ov_info(&file->vorbis, -1);

    if (!info || info->rate <= 0 || info->channels < 1 || info->channels > MaxInputChannels)
    {
        closeFileLocked();
        LightLock_Unlock(&lock);
        return false;
    }

    sampleRate = info->rate;
    inputChannels = info->channels;

    // A buffer lasts the same amount of time whatever the rate of the song is
    framesPerBuffer = (uint32_t)((uint64_t)sampleRate * BufferMilliseconds / 1000);

    if (framesPerBuffer > bufferCapacityFrames)
        framesPerBuffer = bufferCapacityFrames;

    positionFrames = 0;
    lagFrames = 0;
    silenceFrames = 0;

    // A negative lag moves the song forward, a positive one holds it back
    if (offsetMs < 0)
    {
        uint64_t frames = (uint64_t)((double)sampleRate * -offsetMs / 1000.0);

        if (ov_pcm_seek(&file->vorbis, (ogg_int64_t)frames) == 0)
            positionFrames = frames;
    }
    else
    {
        lagFrames = silenceFrames = (uint64_t)((double)sampleRate * offsetMs / 1000.0);
    }

    setupChannelLocked();

    // The buffers are filled before the song starts, so that starting it does
    // not decode anything and cannot delay the music
    preparedBuffers = 0;

    for (int i = 0; i < BufferCount; i++)
    {
        if (!fillBuffer(i))
            break;

        preparedBuffers++;
    }

#ifdef P3DS_DIAG
    {
        FILE *log = fopen("sdmc:/project-3ds-diag.log", "a");

        if (log)
        {
            fprintf(log, "audio: %s, %u Hz, %d channels, %d buffers of %u frames, lag %d ms\n",
                    path.c_str(), (unsigned)sampleRate, inputChannels, preparedBuffers,
                    (unsigned)framesPerBuffer, offsetMs);
            fclose(log);
        }
    }
#endif

    if (preparedBuffers == 0)
    {
        stopLocked();
        LightLock_Unlock(&lock);
        return false;
    }

    LightLock_Unlock(&lock);
    return true;
}

bool AudioEngine::play(const std::string &path, int offsetMs)
{
    if (!prepare(path, offsetMs))
        return false;

    start();
    return true;
}

void AudioEngine::start()
{
    if (!lockReady)
        return;

    LightLock_Lock(&lock);

    if (!file || preparedBuffers == 0 || playing)
    {
        LightLock_Unlock(&lock);
        return;
    }

    for (int i = 0; i < preparedBuffers; i++)
    {
        // The DSP must be able to read the decoded samples as soon as the
        // buffer is queued, so the cache is written back first
        DSP_FlushDataCache(waveBufs[i].data_pcm16, (size_t)waveBufs[i].nsamples * 2 * sizeof(int16_t));
        ndspChnWaveBufAdd(0, &waveBufs[i]);
    }

    preparedBuffers = 0;
    playing = true;
    paused = false;

#ifdef P3DS_DIAG
    // The playback-pacing measurements start when the DSP gets the buffers
    startTick = svcGetSystemTick();
    lastFillTick = startTick;
    intervalTicks = 0;
    maxIntervalTicks = 0;
    intervalCalls = 0;
#endif

    LightLock_Unlock(&lock);
}

bool AudioEngine::fillBuffer(int index)
{
    if (!file)
        return false;

    int16_t *target = (int16_t *)waveBufs[index].data_vaddr;
    uint32_t done = 0;

#ifdef P3DS_DIAG
    uint64_t fillStart = svcGetSystemTick();
#endif

    while (done < framesPerBuffer)
    {
        // A positive lag is played as silence first
        if (silenceFrames > 0)
        {
            uint32_t count = (framesPerBuffer - done) < silenceFrames ?
                             (framesPerBuffer - done) : (uint32_t)silenceFrames;
            memset(target + (size_t)done * 2, 0, (size_t)count * 2 * sizeof(int16_t));
            done += count;
            silenceFrames -= count;
            continue;
        }

        uint32_t chunk = framesPerBuffer - done;

        if (chunk > bufferCapacityFrames)
            chunk = bufferCapacityFrames;

        int read = ov_read(&file->vorbis, (char *)scratch,
                           (int)(chunk * inputChannels * sizeof(int16_t)), nullptr);

        if (read <= 0)
            break;

        uint32_t frames = (uint32_t)read / (inputChannels * sizeof(int16_t));

        if (frames == 0)
            break;

        if (frames > chunk)
            frames = chunk;

        downmix(target + (size_t)done * 2, scratch, frames, inputChannels);
        done += frames;
        positionFrames += frames;
    }

    if (done == 0)
        return false;

    waveBufs[index].nsamples = done;

#ifdef P3DS_DIAG
    uint64_t finished = svcGetSystemTick();
    uint64_t elapsed = finished - fillStart;
    fillTicks += elapsed;

    if (elapsed > maxFillTicks)
        maxFillTicks = elapsed;

    fillCalls++;

    if (lastFillTick)
    {
        uint64_t interval = finished - lastFillTick;
        intervalTicks += interval;

        if (interval > maxIntervalTicks)
            maxIntervalTicks = interval;

        intervalCalls++;
    }

    lastFillTick = finished;
#endif

    return true;
}

void AudioEngine::soundFrameCallback(void *argument)
{
    (void)argument;

    if (decodeRunning)
        LightEvent_Signal(&decodeEvent);
}

void AudioEngine::decodeThread(void *argument)
{
    AudioEngine *engine = (AudioEngine *)argument;

    while (decodeRunning)
    {
        LightLock_Lock(&engine->lock);

        if (engine->playing && !engine->paused)
        {
            for (int i = 0; i < BufferCount; i++)
            {
                if (!engine->playing)
                    break;

                ndspWaveBuf *waveBuf = &engine->waveBufs[i];

                if (waveBuf->status != NDSP_WBUF_DONE)
                    continue;

                if (!engine->fillBuffer(i))
                {
                    // The end of the song was reached, and the buffers that
                    // are left in the queue finish it
                    engine->playing = false;
                    engine->paused = false;
                    engine->closeFileLocked();
                    break;
                }

                DSP_FlushDataCache(waveBuf->data_pcm16, (size_t)waveBuf->nsamples * 2 * sizeof(int16_t));
                ndspChnWaveBufAdd(0, waveBuf);
            }
        }

        LightLock_Unlock(&engine->lock);
        LightEvent_Wait(&decodeEvent);
    }
}

void AudioEngine::stop()
{
    if (!lockReady)
        return;

    LightLock_Lock(&lock);
    stopLocked();
    LightLock_Unlock(&lock);
}

void AudioEngine::pause()
{
    if (!lockReady)
        return;

    LightLock_Lock(&lock);

    // Pausing the channel keeps the queued buffers and their place, so the
    // music resumes exactly where it stopped
    if (playing && !paused)
    {
        ndspChnSetPaused(0, true);
        paused = true;
    }

    LightLock_Unlock(&lock);
}

void AudioEngine::resume()
{
    if (!lockReady)
        return;

    LightLock_Lock(&lock);

    if (playing && paused)
    {
        ndspChnSetPaused(0, false);
        paused = false;
    }

    LightLock_Unlock(&lock);
}

uint32_t AudioEngine::droppedFrames() const
{
    return ndspGetDroppedFrames();
}

uint32_t AudioEngine::frameCount() const
{
    return ndspGetFrameCount();
}

} // namespace p3ds
