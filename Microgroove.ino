#ifdef CARDENZA_TARGET
#include "cardenza/cardenza_hal.h"
#include "cardenza/cardenza_m5_audio.h"
#endif
// ============================================================
// Microgroove — a pocket groovebox for the M5Stack Cardputer-ADV
// by lebiro.studio
//   - 3 synth tracks (saw/sqr/tri/sin + wavetables, 303-style resonant
//     filter, accent, slide); each track switchable 1-3 voices
//     (mono 303 or polyphonic chords)
//   - 8 drum lanes: 808 synth / 909 synth / SD samples, choke groups
//   - 16-step patterns x8, song chaining, live record with quantize
//   - live mic sampling + engine resampling to microSD
//   - project save/load to microSD (GBX v2; loads v1 transparently)
//
// Portions of the synth voice, 808 drums, and audio task are derived
// from qwertyuu/Cardputer-Adv-Tracker (MIT License) - see LICENSE.
// ============================================================
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>

#include "config.h"
#include "sequencer.h"
#include "sampler.h"
#include "wavetable.h"
#include "storage.h"
#include "audio_engine.h"
#include "mic_sampler.h"
#include "ui.h"

void inputInit();
void inputUpdate();

static bool s_sdOk = false;

void setup() {
    auto cfg = M5.config();
#ifdef CARDENZA_TARGET
    Serial.begin(115200);
    delay(200); // Allow USB CDC to reconnect before diagnostic output.
    Serial.println("[Cardenza] startup; probing ES8156");
    const bool cardenzaCodecReady = cardenza_hal_init(32, 16);
    Serial.printf("[Cardenza] codec %s; starting display/keyboard\n",cardenzaCodecReady?"ready":"FAILED");
    cfg.fallback_board = m5::board_t::board_M5Cardputer;
    cfg.internal_imu = false;
#endif
    M5Cardputer.begin(cfg, true);
#ifdef CARDENZA_TARGET
    Serial.printf("[Cardenza] ES8156 %s; I2S16/32fs; no gyro/battery/WS2812; heap=%u\n",
                  cardenzaCodecReady ? "ready" : "FAILED", ESP.getFreeHeap());
    // Feed both ES8156 output channels. playRaw(false) mono input is duplicated by M5Unified.
    M5Cardputer.Speaker.end();
    auto cardenzaSpeaker = M5Cardputer.Speaker.config();
    cardenzaSpeaker.stereo = true;
    M5Cardputer.Speaker.config(cardenzaSpeaker);
    if (!cardenzaCodecReady) {
        M5Cardputer.Display.fillScreen(TFT_BLACK);
        M5Cardputer.Display.setTextColor(TFT_RED);
        M5Cardputer.Display.setCursor(4, 4);
        M5Cardputer.Display.println("ES8156 INIT FAILED");
        while (true) delay(100);
    }
#endif


    uiInit();

    // Speaker / codec
    auto spk = M5Cardputer.Speaker.config();
    spk.sample_rate   = SAMPLE_RATE;
    spk.task_priority = 3;
    spk.dma_buf_count = 4;
    spk.dma_buf_len   = AUDIO_BUF_LEN;
    M5Cardputer.Speaker.config(spk);

#ifdef CARDENZA_TARGET
    cardenza_m5_require(M5Cardputer.Speaker.begin(),"Speaker init FAILED");
#else
    M5Cardputer.Speaker.begin();
#endif
    M5Cardputer.Speaker.setVolume(200);

    // SD (Cardputer-ADV pinout)
    SPI.begin(SD_SPI_CLK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    s_sdOk = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);

    // Modules
    micSamplerInit();                // scratch first, then sample pool
    wavetableInitBuiltins();
    if (s_sdOk) {
        samplerInit();               // also creates /groovebox dirs
        wavetableLoadUserFromSD();
    }
    sequencerInit();
    loadDemoPattern();

    uiSplash();
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
        if (M5Cardputer.BtnA.wasPressed()) break;
        delay(30);
    }

    inputInit();
    audioEngineStart();              // render task on core 0

    if (!s_sdOk) uiStatus("NO SD CARD");
    g_needRedraw = true;
}

void loop() {
    inputUpdate();
    micSamplerUpdate();
    sequencerTick();

    if (g_needRedraw) {
        uiDraw();
        g_needRedraw = false;
    }

    // keep the scope alive on the sound page
    if (g_curPage == PAGE_SOUND && g_playing) {
        static uint32_t last = 0;
        if (millis() - last > 60) { last = millis(); uiDraw(); }
    }

    delay(4);
}
