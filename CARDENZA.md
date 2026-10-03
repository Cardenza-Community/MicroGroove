# Cardenza target

Build with `pio run -e cardenza`. This explicit target requires the current
Cardenza PCB and checks ES8156 identity before initializing it. Keep the original
upstream device target for its hardware. The shared HAL files retain their MIT
license; this does not replace the application's license.

Hardware: ES8156 address0x08 on SDA2/SCL1; Philips16-bit stereo32 clocks/frame;
I2S BCLK41/LRCK43/DATA42. Original Cardputer PDM microphone DATA46/CLK43 is
fitted. Keyboard LED_EN21 is held high (off). Display backlight is GPIO38.
Gyro, battery ADC, charging detection and onboard WS2812 are absent.
The future LED_EN47 PCB revision needs a separate pin-policy update.

The target uses QIO80MHz, 8MB flash and no PSRAM. Install application-only
`firmware.bin` through Cardenza Launcher with its existing Cardenza bootloader;
do not replace the launcher partitions with a stock merged image. Do not erase
shared NVS. M5Unified targets pin separately compiled0.2.17 so Power/LED linker
guards actually intercept initialization;0.2.23+ unity builds require different
guards and must not be silently substituted.

A cooperative render-task pause hands GPIO43 from DAC LRCK to PDM microphone clock. Speaker format is restored through M5Unified-owned I2C before rendering resumes. The upstream splash waits for a key. Sample pool and recording scratch remain internal RAM.


## Validation

The Cardenza target is built separately from the upstream targets. Compilation
and source review do not prove the physical display, keyboard, audio, microphone,
or SD-card behavior; verify these on the target hardware before a release.

The Cardenza build rejects full DATA/NVS partition erases, including Arduino
startup recovery that would clear the Launcher's shared settings. Normal NVS
writes and erases of other explicitly selected partitions are unaffected. An
NVS recovery error requires deliberate repair rather than automatic deletion.
