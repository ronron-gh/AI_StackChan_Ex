// CoreS3 codec register sequences derived from M5Unified (MIT).
// Copyright (c) M5Stack. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include "CoreS3FullDuplexAudio.h"
#ifdef GPTLIVE_FULL_DUPLEX
#include <M5Unified.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include "share/Mutex.h"

namespace {
constexpr i2s_port_t PORT = I2S_NUM_0;
bool ampWrite(uint8_t reg, uint16_t value) {
    value = __builtin_bswap16(value);
    return M5.In_I2C.writeRegister(0x36, reg, (uint8_t*)&value, 2, 400000);
}
bool micWrite(uint8_t reg, uint8_t value) {
    return M5.In_I2C.writeRegister(0x40, reg, &value, 1, 400000);
}
}

bool CoreS3FullDuplexAudio::initCodecs()
{
    if(!micWrite(0x00, 0xFF)) return false;
    static const uint8_t mic[][2] = {
        {0x00,0x41}, {0x01,0x1f}, {0x06,0x00}, {0x07,0x20}, {0x08,0x10},
        {0x09,0x30}, {0x0A,0x30}, {0x20,0x0a}, {0x21,0x2a}, {0x22,0x0a},
        {0x23,0x2a}, {0x02,0xC1}, {0x04,0x01}, {0x05,0x00}, {0x11,0x60},
        {0x40,0x42}, {0x41,0x70}, {0x42,0x70}, {0x43,0x1B}, {0x44,0x1B},
        {0x45,0x00}, {0x46,0x00}, {0x47,0x00}, {0x48,0x00}, {0x49,0x00},
        {0x4A,0x00}, {0x4B,0x00}, {0x4C,0xFF}, {0x01,0x14}
    };
    for(const auto& reg : mic) if(!micWrite(reg[0], reg[1])) return false;
    if(!M5.In_I2C.bitOn(0x58, 0x02, 0x04, 400000)) return false;
    // AW88298: left slot, Philips I2S, 16-bit, BCLK=32fs, I2SSR=5 (24 kHz).
    return ampWrite(0x61, 0x0673) && ampWrite(0x04, 0x4040) &&
           ampWrite(0x05, 0x0008) && ampWrite(0x06, 0x14C5) && ampWrite(0x0C, 0x0064);
}

void CoreS3FullDuplexAudio::disableCodecs()
{
    ampWrite(0x04, 0x4000);
    M5.In_I2C.bitOff(0x58, 0x02, 0x04, 400000);
    micWrite(0x00, 0xFF);
}

bool CoreS3FullDuplexAudio::begin()
{
    if(running.load()) return true;
    if(xSemaphoreTake(mutexAudio, 0) != pdTRUE) return false;
    ownsAudio = true;
    restoreMic = M5.Mic.isRunning();
    restoreSpeaker = M5.Speaker.isRunning();
    M5.Mic.end();
    M5.Speaker.end();
    failed.store(false);
    ring = (uint8_t*)heap_caps_malloc(RING_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ringMutex = xSemaphoreCreateMutex();
    inputQueue = xQueueCreate(8, sizeof(InputFrame));
    if(!ring || !ringMutex || !inputQueue || !initCodecs()){
        Serial.println("[FD] Audio allocation/codec initialization failed");
        end();
        return false;
    }
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 6;
    config.dma_buf_len = FRAME_SAMPLES;
    config.use_apll = true;
    config.tx_desc_auto_clear = true;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    esp_err_t err = i2s_driver_install(PORT, &config, 0, nullptr);
    installed = err == ESP_OK;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = 0;
    pins.bck_io_num = 34;
    pins.ws_io_num = 33;
    pins.data_out_num = 13;
    pins.data_in_num = 14;
    if(err == ESP_OK) err = i2s_set_pin(PORT, &pins);
    if(err != ESP_OK){
        Serial.printf("[FD] I2S init failed: %s\n", esp_err_to_name(err));
        end();
        return false;
    }
    i2s_zero_dma_buffer(PORT);
    readPos = writePos = used = 0;
    rxSamples.store(0); txSamples.store(0); rxDropped.store(0); txUnderruns.store(0);
    capture.store(false);
    running.store(true);
    rxDone.store(false);
    if(xTaskCreate(rxEntry, "fullDuplexRx", 4096, this, 3, &rxTask) != pdPASS){
        rxDone.store(true);
        end();
        return false;
    }
    txDone.store(false);
    if(xTaskCreate(txEntry, "fullDuplexTx", 4096, this, 3, &txTask) != pdPASS){
        txDone.store(true);
        end();
        return false;
    }
#ifdef GPTLIVE_FULL_DUPLEX_TEST_TONE
    constexpr bool testToneEnabled = true;
#else
    constexpr bool testToneEnabled = false;
#endif
    if(testToneEnabled){
    // Optional 100 ms diagnostic tone; RX keeps collecting at the same time.
    int16_t tone[FRAME_SAMPLES];
    for(size_t i = 0; i < FRAME_SAMPLES; ++i) tone[i] = (i % 24 < 12) ? 4000 : -4000;
    for(int i = 0; i < 5; ++i) enqueue((const uint8_t*)tone, sizeof(tone));
    }
    statsAt = millis();
    Serial.println("[FD] CoreS3 TX+RX started: 24000 Hz PCM16 mono, no AEC");
    return true;
}

void CoreS3FullDuplexAudio::end()
{
    capture.store(false);
    running.store(false);
    // I/O waits are bounded; workers finish before queues/ring/I2S are freed.
    while(!rxDone.load() || !txDone.load()) delay(1);
    rxTask = txTask = nullptr;
    if(installed){
        i2s_stop(PORT);
        i2s_driver_uninstall(PORT);
        installed = false;
    }
    if(ownsAudio) disableCodecs();
    if(inputQueue) vQueueDelete(inputQueue);
    if(ringMutex) vSemaphoreDelete(ringMutex);
    heap_caps_free(ring);
    inputQueue = nullptr; ringMutex = nullptr; ring = nullptr;
    readPos = writePos = used = 0;
    outputLevel.store(0);
    inputLevel.store(0);
    failed.store(false);
    // Free worker resources first so M5Unified can allocate its replacement buffers.
    if(ownsAudio){
        if(restoreSpeaker && !M5.Speaker.begin()) Serial.println("[FD] Speaker restore failed");
        if(restoreMic && !M5.Mic.begin()) Serial.println("[FD] Mic restore failed");
        exitMutexAudio();
        ownsAudio = false;
    }
}

void CoreS3FullDuplexAudio::setCapture(bool enabled)
{
    if(enabled == capture.load()) return;
    capture.store(false);
    generation.fetch_add(1);
    if(inputQueue) xQueueReset(inputQueue);
    capture.store(enabled);
}

bool CoreS3FullDuplexAudio::read(InputFrame& frame)
{
    if(!inputQueue || !capture.load()) return false;
    while(xQueueReceive(inputQueue, &frame, 0) == pdTRUE){
        if(frame.generation == generation.load()) return true;
    }
    return false;
}

bool CoreS3FullDuplexAudio::enqueue(const uint8_t* pcm, size_t bytes)
{
    if(!running.load() || failed.load() || bytes % sizeof(int16_t)) return false;
    xSemaphoreTake(ringMutex, portMAX_DELAY);
    bool ok = bytes <= RING_BYTES - used;
    if(ok){
        size_t first = RING_BYTES - writePos;
        if(first > bytes) first = bytes;
        memcpy(ring + writePos, pcm, first);
        memcpy(ring, pcm + first, bytes - first);
        writePos = (writePos + bytes) % RING_BYTES;
        used += bytes;
    }
    xSemaphoreGive(ringMutex);
    if(!ok){
        Serial.println("[FD] Output ring overflow; stopping session");
        failed.store(true);
    }
    return ok;
}

void CoreS3FullDuplexAudio::rxEntry(void* arg) { ((CoreS3FullDuplexAudio*)arg)->receiveLoop(); }
void CoreS3FullDuplexAudio::txEntry(void* arg) { ((CoreS3FullDuplexAudio*)arg)->transmitLoop(); }

void CoreS3FullDuplexAudio::receiveLoop()
{
    InputFrame frame;
    size_t filled = 0;
    uint32_t frameGeneration = generation.load();
    uint32_t lastProgress = millis();
    while(running.load()){
        if(!filled) frameGeneration = generation.load();
        size_t bytes = 0;
        esp_err_t err = i2s_read(PORT, ((uint8_t*)frame.pcm) + filled,
                                 sizeof(frame.pcm) - filled, &bytes, pdMS_TO_TICKS(50));
        if(err != ESP_OK && err != ESP_ERR_TIMEOUT){ failed.store(true); break; }
        if(bytes) lastProgress = millis();
        else if((uint32_t)(millis() - lastProgress) > 500){ failed.store(true); break; }
        filled += bytes;
        if(filled == sizeof(frame.pcm)){
            rxSamples.fetch_add(FRAME_SAMPLES);
            int peak = 0;
            for(const auto sample : frame.pcm){
                const int level = abs((int)sample);
                if(level > peak) peak = level;
            }
            inputLevel.store(peak);
            frame.generation = frameGeneration;
            if(capture.load() && frameGeneration == generation.load() &&
               xQueueSend(inputQueue, &frame, 0) != pdTRUE) rxDropped.fetch_add(1);
            filled = 0;
        }
    }
    rxDone.store(true);
    vTaskDelete(nullptr);
}

void CoreS3FullDuplexAudio::transmitLoop()
{
    int16_t pcm[FRAME_SAMPLES];
    uint32_t lastProgress = millis();
    while(running.load()){
        size_t bytes = sizeof(pcm);
        xSemaphoreTake(ringMutex, portMAX_DELAY);
        if(bytes > used) bytes = used;
        size_t first = RING_BYTES - readPos;
        if(first > bytes) first = bytes;
        memcpy(pcm, ring + readPos, first);
        memcpy(((uint8_t*)pcm) + first, ring, bytes - first);
        readPos = (readPos + bytes) % RING_BYTES;
        used -= bytes;
        xSemaphoreGive(ringMutex);
        if(!bytes) txUnderruns.fetch_add(1);
        memset(((uint8_t*)pcm) + bytes, 0, sizeof(pcm) - bytes);
        const int volume = volume_.load();
        int peak = 0;
        for(auto& sample : pcm){
            sample = (int32_t)sample * volume * volume / (255 * 255);
            const int level = abs((int)sample);
            if(level > peak) peak = level;
        }
        outputLevel.store(peak);
        size_t offset = 0;
        while(offset < sizeof(pcm) && running.load()){
            size_t written = 0;
            esp_err_t err = i2s_write(PORT, ((uint8_t*)pcm) + offset,
                                      sizeof(pcm) - offset, &written, pdMS_TO_TICKS(50));
            if(err != ESP_OK && err != ESP_ERR_TIMEOUT){ failed.store(true); break; }
            offset += written;
            if(written) lastProgress = millis();
            else if((uint32_t)(millis() - lastProgress) > 500){ failed.store(true); break; }
        }
        if(failed.load()) break;
        txSamples.fetch_add(offset / sizeof(int16_t));
    }
    outputLevel.store(0);
    txDone.store(true);
    vTaskDelete(nullptr);
}

void CoreS3FullDuplexAudio::logStats()
{
    if((uint32_t)(millis() - statsAt) < 2000 || !running.load()) return;
    size_t queued;
    xSemaphoreTake(ringMutex, portMAX_DELAY);
    queued = used;
    xSemaphoreGive(ringMutex);
    Serial.printf("[FD] RX=%u TX=%u RXdrop=%u TXempty=%u queued=%u inLevel=%d outLevel=%d capture=%d\n",
                  (unsigned)rxSamples.load(), (unsigned)txSamples.load(),
                  (unsigned)rxDropped.load(), (unsigned)txUnderruns.load(),
                  (unsigned)queued, inputLevel.load(), outputLevel.load(), capture.load());
    statsAt = millis();
}
#endif
