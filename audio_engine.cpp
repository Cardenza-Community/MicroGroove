#ifdef CARDENZA_TARGET
#include "cardenza/cardenza_m5_audio.h"
#endif
// ============================================================
// CardputerGroovebox - audio_engine.cpp
// Render task derived from qwertyuu/Cardputer-Adv-Tracker (MIT),
// extended with 8 drum lanes + sample voices.
// ============================================================
#include "audio_engine.h"
#include "sequencer.h"
#include "sampler.h"
#include <M5Cardputer.h>
#include "mic_sampler.h"
#ifdef CARDENZA_TARGET
#include <atomic>
static std::atomic<bool> pauseRequested{false}, paused{false};
#endif

float g_scopeBuf[SCREEN_W];
volatile int g_scopeIdx = 0;

static int16_t s_bufA[AUDIO_BUF_LEN];
static int16_t s_bufB[AUDIO_BUF_LEN];
static TaskHandle_t s_task = nullptr;

static void audioTask(void*) {
    int16_t* buffers[2] = { s_bufA, s_bufB };
    int cur = 0;

    while (true) {
#ifdef CARDENZA_TARGET
        if (pauseRequested.load()) {
            paused.store(true);
            while (pauseRequested.load()) vTaskDelay(1);
            paused.store(false);
        }
#endif
        int16_t* buf = buffers[cur];

        for (int i = 0; i < AUDIO_BUF_LEN; i++) {
            float mix = 0.0f;

            for (int s = 0; s < NUM_SYNTHS; s++)
                if (!g_synthMute[s]) mix += g_synths[s].render();   // SynthTrack: sums 1..3 voices

            if (!g_drumMute)
                for (int d = 0; d < NUM_DRUM_LANES; d++)
                    mix += g_drumLanes[d].render() * 0.6f;

            mix += g_previewVoice.render();

            // soft clip
            if (mix > 1.0f) mix = 1.0f; else if (mix < -1.0f) mix = -1.0f;
            buf[i] = (int16_t)(mix * 12000.0f);

            if (g_rsmpRemain) {                       // resample tap
                g_scratch[g_scratchWr++] = buf[i];
                g_rsmpRemain--;
            }

            if ((i & 7) == 0 && g_scopeIdx < SCREEN_W)
                g_scopeBuf[g_scopeIdx++] = mix;
        }

        while (!M5Cardputer.Speaker.playRaw(buf, AUDIO_BUF_LEN, SAMPLE_RATE, false, 1, 0))
            vTaskDelay(1);

        cur ^= 1;
    }
}

void audioEngineStart() {
#ifdef CARDENZA_TARGET
    cardenza_m5_require(xTaskCreatePinnedToCore(audioTask,"audio",8192,nullptr,1,&s_task,0)==pdPASS,"Audio task init FAILED");
#else
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 1, &s_task, 0);
#endif
}

#ifdef CARDENZA_TARGET
void audioEnginePause() {
    pauseRequested.store(true);
    while (s_task && !paused.load()) vTaskDelay(1);
}
void audioEngineResume() { pauseRequested.store(false); }
#endif
