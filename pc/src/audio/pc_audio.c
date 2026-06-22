/* pc_audio.c - SDL3 audio backend with dedicated producer thread.
 *
 * Architecture (matches GC):
 *   Game thread:  Na_GameFrame() queues commands via message queues
 *   Audio thread: pc_audio_process_frame() produces samples into ring buffer
 *   SDL stream:   get callback pulls ring buffer -> speakers
 */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_audio_ptr.h"
#include "jaudio_NES/audiothread.h"

#if UINTPTR_MAX > 0xFFFFFFFFu
uintptr_t pc_audio_ptr_base = 0;
#endif

#define PC_AUDIO_SAMPLE_RATE 32000

#define RING_BUF_SAMPLES (32768)
#define RING_BUF_MASK    (RING_BUF_SAMPLES - 1)
#define AUDIO_PRODUCE_THRESHOLD 4480

static s16 ring_buffer[RING_BUF_SAMPLES];
static SDL_atomic_t ring_write_pos;
static SDL_atomic_t ring_read_pos;
static SDL_AudioStream* audio_stream = NULL;
static SDL_AudioDeviceID audio_device = 0;

typedef void (*AIDMACallback)(void);
static AIDMACallback ai_dma_callback = NULL;
static u32 ai_dsp_sample_rate = PC_AUDIO_SAMPLE_RATE;

static SDL_Thread* audio_producer_thread = NULL;
static SDL_atomic_t audio_thread_running;

static void pc_audio_fill_stream(SDL_AudioStream* stream, int additional_amount) {
    if (!stream || additional_amount <= 0) return;

    s16 out[4096];
    int total_samples = additional_amount / (int)sizeof(s16);
    if (total_samples > (int)(sizeof(out) / sizeof(out[0]))) {
        total_samples = (int)(sizeof(out) / sizeof(out[0]));
    }
    total_samples &= ~1;

    u32 wp = (u32)SDL_AtomicGet(&ring_write_pos);
    SDL_MemoryBarrierAcquire();
    u32 rp = (u32)SDL_AtomicGet(&ring_read_pos);
    u32 used = wp - rp;

    if (used > RING_BUF_SAMPLES) {
        rp = wp - RING_BUF_SAMPLES;
        rp &= ~1u;
        used = wp - rp;
    }

    int avail = (int)used;
    avail &= ~1;
    int copy = (avail < total_samples) ? avail : total_samples;
    copy &= ~1;

    for (int i = 0; i < copy; i++) {
        out[i] = ring_buffer[(rp + i) & RING_BUF_MASK];
    }
    if (copy < total_samples) {
        memset(&out[copy], 0, (total_samples - copy) * sizeof(s16));
    }

    SDL_MemoryBarrierRelease();
    SDL_AtomicSet(&ring_read_pos, (int)(rp + copy));

    if (copy > 0 || total_samples > 0) {
        SDL_PutAudioStreamData(stream, out, total_samples * (int)sizeof(s16));
    }
}

static void SDLCALL pc_audio_stream_callback(void* userdata, SDL_AudioStream* stream,
                                             int additional_amount, int total_amount) {
    (void)userdata;
    (void)total_amount;
    pc_audio_fill_stream(stream, additional_amount);
}

static int pc_audio_producer_func(void* data) {
    (void)data;
    while (SDL_AtomicGet(&audio_thread_running)) {
        int fill = pc_audio_get_buffer_fill();
        if (fill < AUDIO_PRODUCE_THRESHOLD) {
            pc_audio_process_frame();
        } else {
            SDL_Delay(1);
        }
    }
    return 0;
}

void pc_audio_start_producer_thread(void) {
    if (audio_producer_thread) return;
    SDL_AtomicSet(&audio_thread_running, 1);
    audio_producer_thread = SDL_CreateThread(pc_audio_producer_func, "AudioProducer", NULL);
    if (audio_producer_thread) {
        printf("[AUDIO] Producer thread started\n");
    } else {
        printf("[AUDIO] Failed to create producer thread: %s\n", SDL_GetError());
    }
}

void AIInit(u8* stack) {
    (void)stack;
    if (audio_stream) return;

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = PC_AUDIO_SAMPLE_RATE;

    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                             pc_audio_stream_callback, NULL);
    if (audio_stream) {
        audio_device = SDL_GetAudioStreamDevice(audio_stream);
        SDL_ResumeAudioStreamDevice(audio_stream);
        printf("[AUDIO] Opened SDL3 stream: freq=%d ch=%d\n", spec.freq, spec.channels);
    } else {
        printf("[AUDIO] Failed to open: %s\n", SDL_GetError());
    }
}

void AIInitDMA(uintptr_t addr, u32 size) {
    s16* src = (s16*)addr;
    u32 n_samples = size / sizeof(s16);
    n_samples &= ~1u;

    u32 wp = (u32)SDL_AtomicGet(&ring_write_pos);
    u32 rp = (u32)SDL_AtomicGet(&ring_read_pos);
    SDL_MemoryBarrierAcquire();
    u32 used = wp - rp;
    u32 free = RING_BUF_SAMPLES - used;

    if (n_samples > free) {
        n_samples = free & ~1u;
    }

    int vol = g_pc_settings.master_volume;
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;

    for (u32 i = 0; i < n_samples; i++) {
        int s = ((int)src[i] * vol) / 100;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        ring_buffer[(wp + i) & RING_BUF_MASK] = (s16)s;
    }

    SDL_MemoryBarrierRelease();
    SDL_AtomicSet(&ring_write_pos, (int)(wp + n_samples));
}

void AIStartDMA(void) {
    if (audio_device) SDL_ResumeAudioDevice(audio_device);
}

void AIStopDMA(void) {
    if (audio_device) SDL_PauseAudioDevice(audio_device);
}

void pc_audio_set_paused(int paused) {
    if (!audio_device) return;
    if (paused) SDL_PauseAudioDevice(audio_device);
    else SDL_ResumeAudioDevice(audio_device);
}

u32  AIGetDMAStartAddr(void) { return 0; }
u16  AIGetDMALength(void) { return 0; }
u32  AIGetStreamTrigger(void) { return 0; }
u32  AIGetStreamSampleCount(void) { return 0; }
void AISetStreamPlayState(u32 state) { (void)state; }
u32  AIGetStreamPlayState(void) { return 0; }
void AISetStreamSampleRate(u32 rate) { (void)rate; }
u32  AIGetStreamSampleRate(void) { return PC_AUDIO_SAMPLE_RATE; }
void AISetStreamVolLeft(u8 vol) { (void)vol; }
void AISetStreamVolRight(u8 vol) { (void)vol; }
u8   AIGetStreamVolLeft(void) { return 0; }
u8   AIGetStreamVolRight(void) { return 0; }
void AIResetStreamSampleCount(void) {}
void AISetDSPSampleRate(u32 rate) { ai_dsp_sample_rate = rate; }
u32  AIGetDSPSampleRate(void) { return ai_dsp_sample_rate; }

void* AIRegisterDMACallback(void* callback) {
    void* old = (void*)ai_dma_callback;
    ai_dma_callback = (AIDMACallback)callback;
    return old;
}

void DSPInit(void) {}
BOOL DSPCheckMailToDSP(void) { return FALSE; }
BOOL DSPCheckMailFromDSP(void) { return FALSE; }
u32  DSPReadMailFromDSP(void) { return 0; }
void DSPSendMailToDSP(u32 mail) { (void)mail; }
void DSPAssertInt(void) {}
void* DSPAddTask(void* task) { return task; }

int pc_audio_get_buffer_fill(void) {
    return SDL_AtomicGet(&ring_write_pos) - SDL_AtomicGet(&ring_read_pos);
}

int pc_audio_is_active(void) {
    return audio_stream != NULL;
}

void pc_audio_shutdown(void) {
    SDL_AtomicSet(&audio_thread_running, 0);
    if (audio_producer_thread) {
        SDL_WaitThread(audio_producer_thread, NULL);
        audio_producer_thread = NULL;
    }
    if (audio_stream) {
        SDL_DestroyAudioStream(audio_stream);
        audio_stream = NULL;
        audio_device = 0;
    }
}
