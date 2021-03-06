/*
 File: JackBridge.h

MIT License

Copyright (c) 2016-2018 Shunji Uno <madhatter68@linux-dtm.ivory.ne.jp>

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
*/

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "JackClient.h"
#include "JackBridge.h"
#include "OSXDaemon.h"

using namespace hula;

/**
 * Construct an OSXDaemon.
 * This will start or join a JACK server and connect all our outputs
 * to our inputs so that we can loopback.
 *
 * All that needs to be done from OSAudio is grab our record device.
 */
OSXDaemon::OSXDaemon(const char *name, int id) : JackClient(name, JACK_PROCESS_CALLBACK), JackBridgeDriverIF(id)
{
    if (attach_shm() < 0)
    {
        fprintf(stderr, "Attaching shared memory failed (id=%d)\n", id);
        exit(1);
    }

    isActive = false;
    isSyncMode = true; // FIXME: should be parameterized
    isVerbose = (getenv("HLOSX_DEBUG")) ? true : false;
    FrameNumber = 0;
    FramesPerBuffer = STRBUFNUM / 2;
    *shmBufferSize = STRBUFSZ;
    *shmSyncMode = 0;

    register_ports(nameAin, nameAout);

    // For DEBUG
    lastHostTime = 0;
    struct mach_timebase_info theTimeBaseInfo;
    mach_timebase_info(&theTimeBaseInfo);
    double theHostClockFrequency = theTimeBaseInfo.denom / theTimeBaseInfo.numer;
    theHostClockFrequency *= 1000000000.0;
    HostTicksPerFrame = theHostClockFrequency / SampleRate;
    if (isVerbose)
    {
        fprintf(stderr, "HulaLoop #%d: Started with samplerate: %d Hz, buffersize: %d bytes\n", instance, SampleRate, BufSize);
    }
}

/**
 * Handle a callback from JACK saying that data is ready.
 */
int OSXDaemon::process_callback(jack_nframes_t nframes)
{
    sample_t *ain[4];
    sample_t *aout[4];

    if (*shmDriverStatus != JB_DRV_STATUS_STARTED)
    {
        // Driver isn't working. Just return zero buffer;
        for (int i = 0; i < 4; i++)
        {
            aout[i] = (sample_t *)jack_port_get_buffer(audioOut[i], nframes);
            bzero(aout[i], STRBUFSZ);
        }
        return 0;
    }

    // For DEBUG
    check_progress();

    if (!isActive)
    {
        ncalls = 0;
        FrameNumber = 0;

        if (isSyncMode)
        {
            *shmSyncMode = 1;
            *shmNumberTimeStamps = 0;
            (*shmSeed)++;
        }

        isActive = true;
        fprintf(stderr, "HulaLoop #%d: Activated with SyncMode = %s, ZeroHostTime = %llx\n",
                instance, isSyncMode ? "Yes" : "No", *shmZeroHostTime);
    }

    if ((FrameNumber % FramesPerBuffer) == 0)
    {
        // FIXME: Should be atomic operation and do memory barrier
        if (*shmSyncMode == 1)
        {
            *shmZeroHostTime = mach_absolute_time();
            *shmNumberTimeStamps = FrameNumber / FramesPerBuffer;
            //(*shmNumberTimeStamps)++;
        }

        if ((!isSyncMode) && isVerbose && ((ncalls++) % 100) == 0)
        {
            fprintf(stderr, "HulaLoop #%d: ZeroHostTime: %llx, %lld, diff:%d\n",
                    instance,  *shmZeroHostTime, *shmNumberTimeStamps,
                    ((int)(mach_absolute_time() + 1000000 - (*shmZeroHostTime))) - 1000000);
        }
    }

    ain[0] = (sample_t *)jack_port_get_buffer(audioIn[0], nframes);
    ain[1] = (sample_t *)jack_port_get_buffer(audioIn[1], nframes);
    ain[2] = (sample_t *)jack_port_get_buffer(audioIn[2], nframes);
    ain[3] = (sample_t *)jack_port_get_buffer(audioIn[3], nframes);
    sendToCoreAudio(ain, nframes);


    aout[0] = (sample_t *)jack_port_get_buffer(audioOut[0], nframes);
    aout[1] = (sample_t *)jack_port_get_buffer(audioOut[1], nframes);
    aout[2] = (sample_t *)jack_port_get_buffer(audioOut[2], nframes);
    aout[3] = (sample_t *)jack_port_get_buffer(audioOut[3], nframes);
    receiveFromCoreAudio(aout, nframes);

    FrameNumber += nframes;

    return 0;
}

// Private members

/**
 * Copy audio data to the downstream buffer to be sent to CoreAudio.
 *
 * @param in TODO
 * @param nframes TODO
 */
int OSXDaemon::sendToCoreAudio(float **in, int nframes)
{
    unsigned int offset = FrameNumber % FramesPerBuffer;
    // FIXME: should be consider buffer overwrapping
    for (int i = 0; i < nframes; i++)
    {
        *(buf_down[0] + (offset + i) * 2) = in[0][i];
        *(buf_down[0] + (offset + i) * 2 + 1) = in[1][i];
        *(buf_down[1] + (offset + i) * 2) = in[2][i];
        *(buf_down[1] + (offset + i) * 2 + 1) = in[3][i];
    }
    return nframes;
}

/**
 * Copy audio data from CoreAudio into our ring buffer.
 * Overwrites the upstream buffer with 0's.
 *
 * @param out TODO
 * @param nframe TODO
 */
int OSXDaemon::receiveFromCoreAudio(float **out, int nframes)
{
    // unsigned int offset = FrameNumber % FramesPerBuffer;
    unsigned int offset = (FrameNumber - nframes) % FramesPerBuffer;
    // FIXME: should be consider buffer overwrapping
    for (int i = 0; i < nframes; i++)
    {
        out[0][i] = *(buf_up[0] + (offset + i) * 2);
        out[1][i] = *(buf_up[0] + (offset + i) * 2 + 1);
        out[2][i] = *(buf_up[1] + (offset + i) * 2);
        out[3][i] = *(buf_up[1] + (offset + i) * 2 + 1);
        *(buf_up[0] + (offset + i) * 2) = 0.0f;
        *(buf_up[0] + (offset + i) * 2 + 1) = 0.0f;
        *(buf_up[1] + (offset + i) * 2) = 0.0f;
        *(buf_up[1] + (offset + i) * 2 + 1) = 0.0f;
    }
    return nframes;
}

/**
 * Detect sync errors where we fall behind.
 *
 */
void OSXDaemon::check_progress()
{
    #if 0
    if (isVerbose && ((ncalls++) % 500) == 0)
    {
        fprintf(stderr, "HulaLoop #%d: FRAME %llu : Write0: %llu Read0: %llu Write1: %llu Read0: %llu\n",
                instance, FrameNumber,
                *shmWriteFrameNumber[0], *shmReadFrameNumber[0],
                *shmWriteFrameNumber[1], *shmReadFrameNumber[1]);
    }
    #endif

    int diff = *shmWriteFrameNumber[0] - FrameNumber;
    int interval = (mach_absolute_time() - lastHostTime) / HostTicksPerFrame;
    if (showmsg)
    {
        if ((diff >= (STRBUFNUM / 2)) || (interval >= BufSize * 2))
        {
            if (isVerbose)
            {
                fprintf(stderr, "WARNING: miss synchronization detected at FRAME %llu (diff=%d, interval=%d)\n",
                        FrameNumber, diff, interval);
            }
            showmsg = false;
        }
    }
    else
    {
        if (diff < (STRBUFNUM / 2))
        {
            showmsg = true;
        }
    }
    lastHostTime = mach_absolute_time();
}

/**
 * Destruct OSXDaemon.
 */
OSXDaemon::~OSXDaemon()
{
    fprintf(stderr, "OSXDaemon destructor called\n");
}