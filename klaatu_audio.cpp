
#define LOG_TAG "KLAATUAUD"
#include <pthread.h>
#include <sys/resource.h>
#include <cutils/sched_policy.h>
#include <system/audio.h> // Most definitions from: system/core/include/system/audio.h
#include <binder/IPCThreadState.h>
#include <media/IAudioFlinger.h>
#include <media/AudioSystem.h>
#include <private/media/AudioTrackShared.h>
#include "klaatu_api.h"
extern "C" void console_print( const char *str, ... );

using namespace android;

class KlaatuAudio {
public:
    enum event_type {
        EVENT_UNDERRUN = 1,   // PCM buffer underrun occured.
        EVENT_BUFFER_END = 5  // Playback head is at the end of the buffer.
    };
    KlaatuAudio(void *arg_user, int arg_sampleRate, int arg_bytes, int arg_channels, int arg_frame_size, int arg_num_updates, getdata_t arg_get_data);
    ~KlaatuAudio();
    void                audiostop();
    void                ThreadWorker(void);
    void                audioCallback(int event);
    volatile int        mRunning;
    pthread_t           mThread;
    sp<IAudioTrack>     mAudio;
    sp<IMemory>         mCblkMemory;
    audio_track_cblk_t* mCblk;
    audio_format_t      mFormat;
    audio_stream_type_t mStreamType;
    bool                mActive;                // protected by mLock
    int                 mSessionId;
    mutable Mutex       mLock;
    int                 mPreviousPriority;          // before start()
    SchedPolicy         mPreviousSchedulingGroup;
    void*               mUser;
    getdata_t           mGetData;
};

KlaatuAudio::~KlaatuAudio()
{
    audiostop();
    mAudio.clear();
    IPCThreadState::self()->flushCommands();
    AudioSystem::releaseAudioSessionId(mSessionId);
}

void KlaatuAudio::audiostop()
{
    AutoMutex lock(mLock);
    if (mActive) {
        mActive = false;
        mCblk->cv.signal();
        mAudio->stop();
        setpriority(PRIO_PROCESS, 0, mPreviousPriority);
        set_sched_policy(0, mPreviousSchedulingGroup);
    }
}

void KlaatuAudio::audioCallback(int event)
{
    if (event == EVENT_UNDERRUN) {
        console_print("AUDIO: underrun event\n");
        mRunning = 0;
    }
    else
        console_print("AUDIO: unknown event %d\n", event);
}

void KlaatuAudio::ThreadWorker(void)
{
    status_t err = 0;
    audio_track_cblk_t* cblk = mCblk;

    while (mRunning) {
        if (mActive && (cblk->framesAvailable() == cblk->frameCount)) {
            if (!(android_atomic_or(CBLK_UNDERRUN_ON, &cblk->flags) & CBLK_UNDERRUN_MSK)) {
                audioCallback(EVENT_UNDERRUN);
                if (cblk->server == cblk->frameCount)
                    audioCallback(EVENT_BUFFER_END);
            }
        }
        uint32_t TTsize;
        cblk->lock.lock();
        while ((TTsize = cblk->framesAvailable_l()) == 0) {
            if ((cblk->flags & CBLK_INVALID_MSK) || !mActive) {
                err = 1;
                break;
            }
            cblk->cv.waitRelative(cblk->lock, milliseconds(cblk->bufferTimeoutMs));
        }
        cblk->lock.unlock();
        if (!mActive || err) {
            err =  1;
            console_print("[%s:%d] Error loop %x\n", __FUNCTION__, __LINE__, err);
            mRunning = 0;
            break;
        }
        uint32_t u = cblk->user;
        void *TTdata = cblk->buffer(u);
        uint32_t bufferEnd = cblk->userBase + cblk->frameCount - u;
//console_print("[%s:%d] avail %d end %d\n", __FUNCTION__, __LINE__, TTsize, bufferEnd);
        /* Make sure that we don't wrap around end of buffer area
         * on a single call to copy samples */
        if (TTsize > bufferEnd)
            TTsize = bufferEnd;
        mGetData(mUser, TTdata, TTsize);
        if (mFormat == AUDIO_FORMAT_PCM_8_BIT) {
            /* as a convenience to library user, support unpacking of
             * 8 bit samples into 16 bit values.  (AudioFlinger only
             * supports 16 bit samples)
             */
            size_t count = (TTsize * cblk->frameSize)/2;
            int16_t *dst = ((int16_t *)TTdata) + count;
            uint8_t *src = ((uint8_t *)TTdata) + count;
            while (count--)
                *--dst = (*--src - 0x80) << 8;
        }
        //static int jca = 0;
        //if (jca++ > 100) {
//console_print("[%s:%d]ZZZZZZZZ nothing to doooooooo \n", __FUNCTION__, __LINE__);
            //usleep(WAIT_PERIOD_MS*1000);
            //continue;
        //}
        cblk->stepUser(TTsize);
        if (mActive && (cblk->flags & CBLK_DISABLED_MSK)) {
            android_atomic_and(~CBLK_DISABLED_ON, &cblk->flags);
            console_print("releaseBuffer() track name=%#x disabled, restarting", cblk->mName);
            mAudio->start();
        }
    }
    audiostop();
}

static void* thread_function(void* arg)
{
    ((KlaatuAudio *)arg)->ThreadWorker();
    return NULL;
}

KlaatuAudio::KlaatuAudio(void *arg_user, int arg_sampleRate, int arg_bytes,
    int arg_channels, int arg_frame_size, int arg_num_updates, getdata_t arg_get_data)
{
    int afSampleRate, afFrameCount;
    uint32_t afLatency;
    status_t status;
    audio_stream_type_t streamType = AUDIO_STREAM_DEFAULT;
    float tvolume_LEFT = 1.0f;
    float tvolume_RIGHT = 1.0f;
    uint32_t tchanmask = audio_channel_out_mask_from_count(arg_channels);

    mPreviousPriority = ANDROID_PRIORITY_NORMAL;
    mPreviousSchedulingGroup = SP_DEFAULT;
    mUser = arg_user;
    mRunning = 1;
    mSessionId = AUDIO_SESSION_OUTPUT_MIX;
    mStreamType = (audio_stream_type_t)AUDIO_STREAM_MUSIC;
    mActive = true;
    mGetData = arg_get_data;
    mFormat = (arg_bytes == 1) ? AUDIO_FORMAT_PCM_8_BIT : AUDIO_FORMAT_PCM_16_BIT;

    audio_io_handle_t output = AudioSystem::getOutput(mStreamType,
        arg_sampleRate, mFormat, tchanmask, AUDIO_OUTPUT_FLAG_NONE);
    if (AudioSystem::getLatency(output, mStreamType, &afLatency) != NO_ERROR
     || AudioSystem::getSamplingRate(output, mStreamType, &afSampleRate) != NO_ERROR
     || AudioSystem::getFrameCount(output, mStreamType, &afFrameCount) != NO_ERROR)
        return; // ALC_FALSE; //NO_INIT;
    uint32_t minBufCount = (afLatency * afSampleRate) / (1000 * afFrameCount);
    console_print("FC=%d: BC=%d, SR=%d, LAT=%d", afFrameCount, minBufCount, afSampleRate, afLatency);
    if (minBufCount < 2)
        minBufCount = 2;

    /****** connect with AudioFlinger ******/
    const sp<IAudioFlinger>& audioFlinger = AudioSystem::get_audio_flinger();
    if (audioFlinger == 0) {
        console_print("Could not get audioflinger");
        return; // ALC_FALSE; //NO_INIT;
    }
    mAudio = audioFlinger->createTrack(getpid(), mStreamType, afSampleRate,
        mFormat, tchanmask, afFrameCount * minBufCount * arg_num_updates,
        IAudioFlinger::TRACK_DEFAULT, 0, output, -1, &mSessionId, &status);
    if (mAudio == 0) {
        console_print("AudioFlinger could not create track, status: %d", status);
        return; // ALC_FALSE; //status;
    }
    mCblkMemory = mAudio->getCblk();
    if (mCblkMemory == 0) {
        console_print("Could not get control block");
        return; // ALC_FALSE; //NO_INIT;
    }
    mCblk = static_cast<audio_track_cblk_t*>(mCblkMemory->pointer());
    android_atomic_or(CBLK_DIRECTION_OUT, &mCblk->flags);
    mCblk->buffers = (char*)mCblk + sizeof(audio_track_cblk_t);
    mCblk->setVolumeLR((uint32_t(uint16_t(tvolume_RIGHT * 0x1000)) << 16)
        | uint16_t(tvolume_LEFT * 0x1000));
    mCblk->setSendLevel(0.0f);
    mAudio->attachAuxEffect(0);
    mCblk->bufferTimeoutMs = MAX_STARTUP_TIMEOUT_MS;
    mCblk->waitTimeMs = 0;
    AudioSystem::acquireAudioSessionId(mSessionId);
    mPreviousPriority = getpriority(PRIO_PROCESS, 0);
    get_sched_policy(0, &mPreviousSchedulingGroup);
    androidSetThreadPriority(0, ANDROID_PRIORITY_AUDIO);

    audio_track_cblk_t* cblk = mCblk;
    cblk->lock.lock();
    android_atomic_and(~CBLK_DISABLED_ON, &cblk->flags);
    cblk->lock.unlock();
    mAudio->start();
    pthread_create(&mThread, NULL, thread_function, this);
}

/*
 * Exported API to applications
 */
void *KlaatuAudioOpen(void *arg_user, int arg_sampleRate, int arg_bytes, 
   int arg_channels, int arg_frame_size, int arg_num_updates, getdata_t arg_get_data)
{
    return new KlaatuAudio(arg_user, arg_sampleRate, arg_bytes, arg_channels, 
        arg_frame_size, arg_num_updates, arg_get_data);
}

void KlaatuAudioStop(void *arg)
{
    KlaatuAudio *aa = (KlaatuAudio *)arg;
    if (aa->mRunning) {
        aa->mRunning = 0;
        pthread_join(aa->mThread, NULL);
    }
}

void KlaatuAudioDelete(void *arg)
{
    if (arg != 0) {
        delete (KlaatuAudio *)arg;
    }
}
