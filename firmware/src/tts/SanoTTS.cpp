#if defined(USE_SANOTTS)

#include "SanoTTS.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include "esp_heap_caps.h"
#include "esp_timer.h"

extern "C" {
#include "saanotts.h"
#include "saanotts_stream.h"
#include "saan_kanji.h"
#include "jdict.h"
}

// These generated headers define the embedded arrays and must be included by
// exactly one translation unit.
#include "saan_model_blob.h"
#include "saan_dict_blob.h"

namespace {
constexpr size_t kArenaBytes = 176 * 1024;
constexpr int32_t kMaxIds = 350;
constexpr size_t kMaxTextBytes = 1023;
constexpr size_t kPcmSamples = SAAN_CHUNK * SAAN_HOP;
constexpr size_t kPrerollSamples = 4 * kPcmSamples;
constexpr size_t kRingBuffers = 3;
constexpr size_t kLevelWindowSamples = 256;
#if defined(SANOTTS_PLAYBACK_SAMPLE_RATE)
constexpr uint32_t kPlaybackSampleRate = SANOTTS_PLAYBACK_SAMPLE_RATE;
#else
constexpr uint32_t kPlaybackSampleRate = SAAN_SR;
#endif
static_assert(kPlaybackSampleRate > 0, "SanoTTS playback sample rate must be greater than zero");
constexpr int64_t kMaxSegmentPlaybackUs =
    static_cast<int64_t>(SAAN_SR) * 30 * 1000000 / kPlaybackSampleRate;
static_assert(kArenaBytes >= SAAN_KANJI_WORKBYTES, "SanoTTS arena is too small");

saan_weights g_weights;
jdict_t g_dictionary;
void *g_arenaMemory = nullptr;
float g_pcm[kPcmSamples];

size_t utf8Length(const char *text, size_t remaining) {
    const uint8_t lead = static_cast<uint8_t>(text[0]);
    size_t length = 0;
    if (lead < 0x80) length = 1;
    else if ((lead & 0xe0) == 0xc0) length = 2;
    else if ((lead & 0xf0) == 0xe0) length = 3;
    else if ((lead & 0xf8) == 0xf0) length = 4;
    if (!length || length > remaining) return 0;
    for (size_t i = 1; i < length; ++i) {
        if ((static_cast<uint8_t>(text[i]) & 0xc0) != 0x80) return 0;
    }
    return length;
}

bool isDelimiter(const char *text, size_t length) {
    if (length == 1) {
        switch (text[0]) {
        case '\n': case '\r': case '.': case ',': case '!': case '?': case ';': case ':':
            return true;
        default:
            return false;
        }
    }
    if (length != 3) return false;
    static const char *const delimiters[] = {u8"。", u8"、", u8"！", u8"？", u8"，", u8"；", u8"："};
    for (const char *delimiter : delimiters) {
        if (memcmp(text, delimiter, 3) == 0) return true;
    }
    return false;
}

// Returns a UTF-8 boundary, preferring the first punctuation/newline within maxBytes.
size_t nextSegmentEnd(const String& text, size_t start, size_t maxBytes) {
    const char *bytes = text.c_str();
    const size_t total = text.length();
    size_t pos = start;
    while (pos < total) {
        const size_t length = utf8Length(bytes + pos, total - pos);
        if (!length) return 0;
        if (pos + length - start > maxBytes) break;
        pos += length;
        if (isDelimiter(bytes + pos - length, length)) return pos;
    }
    return pos;
}

size_t retrySplitPoint(const String& text) {
    const size_t target = text.length() / 2;
    size_t pos = 0;
    size_t delimiter = 0;
    while (pos < text.length()) {
        const size_t length = utf8Length(text.c_str() + pos, text.length() - pos);
        if (!length) return 0;
        pos += length;
        if (pos <= target && isDelimiter(text.c_str() + pos - length, length)) delimiter = pos;
        if (pos >= target) return delimiter ? delimiter : pos;
    }
    return 0;
}

bool openModel() {
    if ((reinterpret_cast<uintptr_t>(g_saan_model_blob) & 15u) != 0u) {
        Serial.println("SanoTTS: model blob is not 16-byte aligned");
        return false;
    }
    const saan_status status = saan_weights_open(&g_weights, g_saan_model_blob,
                                                 sizeof(g_saan_model_blob));
    if (status != SAAN_OK) {
        Serial.printf("SanoTTS: model open failed: %s\n", saan_strerror(status));
        return false;
    }
    return true;
}

bool openDictionary() {
    if (jdict_open(&g_dictionary, saan_dict_blob, sizeof(saan_dict_blob)) != 0) return false;
    return g_dictionary.n_entries == 44000 && g_dictionary.lsize == 1377 &&
           g_dictionary.rsize == 1377 && g_dictionary.matrix_q &&
           g_dictionary.matrix_rmap && g_dictionary.matrix_cmap &&
           g_dictionary.char_runs && g_dictionary.unk;
}

#if defined(SANOTTS_BUFFERED_PLAYBACK)
// One slot owns the complete PCM for one text segment. M5Unified plays the
// PCM asynchronously, so the slot must stay alive until that queued playback
// has finished. The envelope follows the same lifetime for lip sync.
struct BufferedSegment {
    ~BufferedSegment() { reset(); }

    void reset() {
        if (pcm) heap_caps_free(pcm);
        if (envelope) heap_caps_free(envelope);
        pcm = nullptr;
        envelope = nullptr;
        samples = 0;
        envelopeCount = 0;
        maxPullUs = 0;
    }

    bool allocate(size_t expected) {
        reset();
        pcm = static_cast<int16_t *>(heap_caps_malloc(
            expected * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        envelopeCount = (expected + kLevelWindowSamples - 1) / kLevelWindowSamples;
        envelope = static_cast<uint16_t *>(heap_caps_calloc(
            envelopeCount, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (pcm && envelope) return true;
        Serial.printf("SanoTTS: cannot allocate buffered segment PCM=%u envelope=%u bytes\n",
                      static_cast<unsigned>(expected * sizeof(int16_t)),
                      static_cast<unsigned>(envelopeCount * sizeof(uint16_t)));
        reset();
        return false;
    }

    int16_t *pcm = nullptr;
    uint16_t *envelope = nullptr;
    size_t samples = 0;
    size_t envelopeCount = 0;
    int64_t maxPullUs = 0;
};

enum class PrepareStatus { Ready, Split, NoMemory, Error };

// Parse and synthesize one complete segment into output. This function does
// not play audio; after it returns, the worker can hand the resulting PCM to
// M5Unified and use the other slot to prepare the following segment.
PrepareStatus prepareBufferedSegment(const String& text, BufferedSegment& output,
                                     String& first, String& second) {
    int32_t ids[kMaxIds];
    int32_t count = 0;
    int tokens = 0;
    const saan_kanji_status parsed = saan_kanji_to_ids(
        &g_dictionary, text.c_str(), text.length(), g_arenaMemory, kArenaBytes,
        ids, kMaxIds, &count, &tokens);
    if (parsed != SAAN_KANJI_OK || count <= 0) {
        if (parsed == SAAN_KANJI_ERR_TOO_LONG || parsed == SAAN_KANJI_ERR_IDS) {
            const size_t split = retrySplitPoint(text);
            if (split > 0 && split < text.length()) {
                first = text.substring(0, split);
                second = text.substring(split);
                first.trim();
                second.trim();
                Serial.printf("SanoTTS: retry split at %u bytes (%s)\n",
                              static_cast<unsigned>(split), saan_kanji_strerror(parsed));
                return PrepareStatus::Split;
            }
        }
        Serial.printf("SanoTTS: parse failed: %s\n", saan_kanji_strerror(parsed));
        return PrepareStatus::Error;
    }

    saan_arena arena;
    saan_stream stream = {};
    saan_arena_init(&arena, g_arenaMemory, kArenaBytes);
    saan_status status = saan_stream_init(&stream, &g_weights, &arena, ids, count, SAAN_S_V);
    if (status != SAAN_OK || arena.failed ||
        arena.used != saan_stream_arena_used(count) || stream.n_frames <= 0) {
        return PrepareStatus::Error;
    }
    const uint64_t expectedWide = static_cast<uint64_t>(stream.n_frames) * SAAN_HOP;
    if (expectedWide > static_cast<uint64_t>(SAAN_SR) * 30 ||
        expectedWide > SIZE_MAX / sizeof(int16_t)) {
        return PrepareStatus::Error;
    }
    const size_t expected = static_cast<size_t>(expectedWide);
    if (!output.allocate(expected)) return PrepareStatus::NoMemory;

    size_t samples = 0;
    bool ended = false;
    for (size_t i = 0; i <= (expected + kPcmSamples - 1) / kPcmSamples; ++i) {
        int32_t frames = 0;
        const int64_t pullStarted = esp_timer_get_time();
        status = saan_stream_pull(&stream, g_pcm, &frames);
        const int64_t pullUs = esp_timer_get_time() - pullStarted;
        if (pullUs > output.maxPullUs) output.maxPullUs = pullUs;
        if (status != SAAN_OK || arena.failed || frames < 0 || frames > SAAN_CHUNK) {
            output.reset();
            return PrepareStatus::Error;
        }
        if (frames == 0) {
            ended = true;
            break;
        }
        const size_t n = static_cast<size_t>(frames) * SAAN_HOP;
        if (samples + n > expected) {
            output.reset();
            return PrepareStatus::Error;
        }
        for (size_t j = 0; j < n; ++j) {
            if (!std::isfinite(g_pcm[j])) {
                output.reset();
                return PrepareStatus::Error;
            }
            const float scaled = g_pcm[j] * 32767.0f;
            long v;
            if (scaled > 32767.0f) v = 32767;
            else if (scaled < -32768.0f) v = -32768;
            else v = lrintf(scaled);
            output.pcm[samples + j] = static_cast<int16_t>(v);
            const uint16_t magnitude = static_cast<uint16_t>(
                std::min(abs(static_cast<int>(output.pcm[samples + j])), 32767));
            uint16_t& peak = output.envelope[(samples + j) / kLevelWindowSamples];
            if (magnitude > peak) peak = magnitude;
        }
        samples += n;
        vTaskDelay(1);
    }
    if (!ended || samples != expected) {
        output.reset();
        return PrepareStatus::Error;
    }
    output.samples = samples;
    Serial.printf("SanoTTS: segment bytes=%u ids=%d PCM=%u bytes max_pull=%.2f ms\n",
                  static_cast<unsigned>(text.length()), static_cast<int>(count),
                  static_cast<unsigned>(samples * sizeof(int16_t)),
                  output.maxPullUs / 1000.0);
    return PrepareStatus::Ready;
}
#else
class StreamingOutput {
public:
    ~StreamingOutput() {
        if (!finished_ && started_ && M5.Speaker.isRunning()) M5.Speaker.end();
        if (storage_) heap_caps_free(storage_);
    }

    bool allocate(size_t expected) {
        prerollCapacity_ = std::min(expected, kPrerollSamples);
        const size_t count = prerollCapacity_ + kRingBuffers * kPcmSamples;
        storage_ = static_cast<int16_t *>(heap_caps_malloc(
            count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!storage_) Serial.println("SanoTTS: cannot allocate streaming buffers");
        return storage_ != nullptr;
    }

    int16_t *destination(size_t count) {
        if (!started_) return count <= prerollCapacity_ - prerollFill_ ? storage_ + prerollFill_ : nullptr;
        return count <= kPcmSamples ? storage_ + prerollCapacity_ + ring_ * kPcmSamples : nullptr;
    }

    bool submit(size_t count) {
        if (!started_) {
            prerollFill_ += count;
            if (prerollFill_ < prerollCapacity_) return true;
            if (!queue(storage_, prerollFill_)) return false;
            started_ = true;
            return true;
        }
        int16_t *p = storage_ + prerollCapacity_ + ring_ * kPcmSamples;
        if (!queue(p, count)) return false;
        ring_ = (ring_ + 1) % kRingBuffers;
        return true;
    }

    bool finish(size_t expected) {
        if (!started_ || sent_ != expected) return false;
        const int64_t timeout = esp_timer_get_time() + 5000000;
        while (M5.Speaker.isPlaying(0)) {
            if (esp_timer_get_time() > timeout) return false;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        const auto cfg = M5.Speaker.config();
        const uint32_t drain =
            (cfg.dma_buf_len * cfg.dma_buf_count * 1000 + kPlaybackSampleRate - 1) /
                kPlaybackSampleRate +
            20;
        vTaskDelay(pdMS_TO_TICKS(drain));
        finished_ = true;
        return true;
    }

    uint32_t queueEmptyEvents() const { return queueEmptyEvents_; }

private:
    bool queue(const int16_t *data, size_t count) {
        const int64_t timeout = esp_timer_get_time() + 5000000;
        while (M5.Speaker.isPlaying(0) >= 2) {
            if (esp_timer_get_time() > timeout) return false;
            vTaskDelay(1);
        }
        if (!M5.Speaker.isRunning()) return false;
        if (sent_ && !M5.Speaker.isPlaying(0)) ++queueEmptyEvents_;
        if (!M5.Speaker.playRaw(data, count, kPlaybackSampleRate, false, 1, 0, false)) return false;
        sent_ += count;
        return true;
    }

    int16_t *storage_ = nullptr;
    size_t prerollCapacity_ = 0, prerollFill_ = 0, ring_ = 0, sent_ = 0;
    uint32_t queueEmptyEvents_ = 0;
    bool started_ = false, finished_ = false;
};
#endif
}

SanoTTS::SanoTTS() {
    isOfflineService = true;
    requestMutex_ = xSemaphoreCreateMutex();
    requestReady_ = xSemaphoreCreateBinary();
    requestDone_ = xSemaphoreCreateBinary();
    if (!requestMutex_ || !requestReady_ || !requestDone_ ||
        xTaskCreatePinnedToCore(workerEntry, "SanoTTS", 16 * 1024, this, 2, &worker_, 1) != pdPASS) {
        Serial.println("SanoTTS: cannot create worker");
        worker_ = nullptr;
    }
}

bool SanoTTS::isReady() const { return ready_; }
int SanoTTS::getLevel() {
#if defined(SANOTTS_BUFFERED_PLAYBACK)
    const int64_t now = esp_timer_get_time();
    int level = 0;
    portENTER_CRITICAL(&levelMux_);
    for (const auto& state : levelStates_) {
        if (!state.envelope || now < state.startUs || now >= state.endUs) continue;
        const uint64_t sample = static_cast<uint64_t>(now - state.startUs) *
                                kPlaybackSampleRate / 1000000;
        const size_t index = static_cast<size_t>(sample / kLevelWindowSamples);
        if (index < state.count) level = state.envelope[index];
        break;
    }
    portEXIT_CRITICAL(&levelMux_);
    return level;
#else
    return level_;
#endif
}

#if defined(SANOTTS_BUFFERED_PLAYBACK)
void SanoTTS::setLevelState(size_t slot, const uint16_t *envelope, size_t count,
                            int64_t startUs, int64_t endUs) {
    portENTER_CRITICAL(&levelMux_);
    levelStates_[slot].envelope = envelope;
    levelStates_[slot].count = count;
    levelStates_[slot].startUs = startUs;
    levelStates_[slot].endUs = endUs;
    portEXIT_CRITICAL(&levelMux_);
}

void SanoTTS::clearLevelState(size_t slot) {
    portENTER_CRITICAL(&levelMux_);
    levelStates_[slot].envelope = nullptr;
    levelStates_[slot].count = 0;
    levelStates_[slot].startUs = 0;
    levelStates_[slot].endUs = 0;
    portEXIT_CRITICAL(&levelMux_);
}
#endif

void SanoTTS::stream(String text) {
    if (!worker_ || text.length() == 0) return;
    xSemaphoreTake(requestMutex_, portMAX_DELAY);
    requestText_ = text;
    xSemaphoreGive(requestReady_);
    xSemaphoreTake(requestDone_, portMAX_DELAY);
    xSemaphoreGive(requestMutex_);
}

void SanoTTS::workerEntry(void *arg) { static_cast<SanoTTS *>(arg)->workerLoop(); }

void SanoTTS::workerLoop() {
    ready_ = initialize();
    for (;;) {
        xSemaphoreTake(requestReady_, portMAX_DELAY);
        if (ready_) speak(requestText_);
        else Serial.println("SanoTTS: unavailable; speech request ignored");
        requestText_ = "";
        level_ = 0;
        xSemaphoreGive(requestDone_);
    }
}

bool SanoTTS::initialize() {
    if (!openModel() || !openDictionary() || !saan_kanji_init()) return false;
    g_arenaMemory = heap_caps_aligned_alloc(16, kArenaBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const bool internalArena = g_arenaMemory != nullptr;
    if (!g_arenaMemory)
        g_arenaMemory = heap_caps_aligned_alloc(16, kArenaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_arenaMemory) {
        Serial.println("SanoTTS: cannot allocate inference arena");
        return false;
    }
    Serial.printf("SanoTTS: arena=%s %u bytes at %p\n",
                  internalArena ? "internal RAM" : "PSRAM",
                  static_cast<unsigned>(kArenaBytes), g_arenaMemory);
    Serial.println("SanoTTS: ready");
    return true;
}

bool SanoTTS::speak(const String& text) {
    const bool micWasRunning = M5.Mic.isRunning();
    const bool speakerWasRunning = M5.Speaker.isRunning();
    const auto previousSpeakerConfig = M5.Speaker.config();
    if (micWasRunning) M5.Mic.end();
    if (speakerWasRunning) M5.Speaker.end();
    auto cfg = M5.Speaker.config();
    cfg.sample_rate = SAAN_SR;
    cfg.stereo = false;
    // Keep about 186 ms of audio in DMA while retaining the application's
    // original task priority and core assignment.
    //cfg.task_priority = 4;
    //cfg.dma_buf_len = 256;
    //cfg.dma_buf_count = 8;
    M5.Speaker.config(cfg);
    bool ok = M5.Speaker.begin() && M5.Speaker.isEnabled();
#if defined(SANOTTS_BUFFERED_PLAYBACK)
    std::deque<String> pending;
    size_t start = 0;
    while (ok && start < text.length()) {
        const size_t end = nextSegmentEnd(text, start, kMaxTextBytes);
        if (end <= start) {
            Serial.println("SanoTTS: invalid UTF-8 input");
            ok = false;
            break;
        }
        String segment = text.substring(start, end);
        segment.trim();
        if (segment.length()) pending.push_back(segment);
        start = end;
    }

    // M5Unified consumes one slot in its speaker task while this SanoTTS
    // worker fills the other slot with the next segment. Synthesis itself is
    // still serial and continues to use the single shared inference arena.
    BufferedSegment slots[2];
    // queued[i] means that the speaker queue may still reference slots[i].
    // Such a slot must not be freed or overwritten yet.
    bool queued[2] = {false, false};
    size_t slot = 0;
    int64_t nextPlaybackStartUs = 0;
    const auto waitForQueueBelow = [&](size_t limit, int64_t timeoutUs) {
        while (M5.Speaker.isPlaying(0) >= limit) {
            if (esp_timer_get_time() > timeoutUs) return false;
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        return true;
    };
    const auto releaseSlot = [&](size_t index) {
        clearLevelState(index);
        slots[index].reset();
        queued[index] = false;
    };

    while (ok && !pending.empty()) {
        if (queued[slot]) {
            // Both slots have been used. Wait until the older queued segment
            // is no longer needed, then recycle its storage for new PCM.
            ok = waitForQueueBelow(2, esp_timer_get_time() + kMaxSegmentPlaybackUs + 5000000);
            if (!ok) break;
            releaseSlot(slot);
        }

        String first, second;
        // After the first playRaw() call, this runs while the speaker task is
        // playing the preceding slot. This is the look-ahead generation that
        // hides most of the synthesis delay between text segments.
        PrepareStatus prepared = prepareBufferedSegment(pending.front(), slots[slot], first, second);
        if (prepared == PrepareStatus::Split) {
            pending.pop_front();
            if (second.length()) pending.push_front(second);
            if (first.length()) pending.push_front(first);
            continue;
        }
        if (prepared == PrepareStatus::NoMemory && M5.Speaker.isPlaying(0)) {
            // Two buffered segments did not fit in PSRAM. Finish playback and
            // release its PCM before retrying, which falls back to serial
            // generate-then-play operation instead of aborting the utterance.
            Serial.println("SanoTTS: waiting for playback to release PSRAM before retry");
            ok = waitForQueueBelow(1, esp_timer_get_time() +
                                   2 * kMaxSegmentPlaybackUs + 5000000);
            if (!ok) break;
            for (size_t i = 0; i < 2; ++i) {
                if (queued[i]) releaseSlot(i);
            }
            nextPlaybackStartUs = 0;
            prepared = prepareBufferedSegment(pending.front(), slots[slot], first, second);
        }
        if (prepared != PrepareStatus::Ready) {
            ok = false;
            break;
        }
        pending.pop_front();

        // stop_current=false queues this PCM for asynchronous playback. Once
        // queued, control returns to this worker immediately; switching slot
        // below lets the next loop synthesize the following segment in
        // parallel with audio playback by M5Unified's speaker task.
        ok = waitForQueueBelow(2, esp_timer_get_time() + kMaxSegmentPlaybackUs + 5000000);
        if (!ok || !M5.Speaker.isRunning() ||
            !M5.Speaker.playRaw(slots[slot].pcm, slots[slot].samples,
                                kPlaybackSampleRate, false, 1, 0, false)) {
            ok = false;
            break;
        }
        const int64_t now = esp_timer_get_time();
        const int64_t previousEndUs = nextPlaybackStartUs;
        const int64_t playbackStartUs = std::max(now, nextPlaybackStartUs);
        const int64_t playbackEndUs = playbackStartUs +
            static_cast<int64_t>(slots[slot].samples) * 1000000 / kPlaybackSampleRate;
        setLevelState(slot, slots[slot].envelope, slots[slot].envelopeCount,
                      playbackStartUs, playbackEndUs);
        queued[slot] = true;
        nextPlaybackStartUs = playbackEndUs;
        const int64_t queuedAheadUs = std::max<int64_t>(0, previousEndUs - now);
        const int64_t lateUs = previousEndUs ? std::max<int64_t>(0, now - previousEndUs) : 0;
        Serial.printf("SanoTTS: queued slot=%u samples=%u ahead=%.2f ms late=%.2f ms\n",
                      static_cast<unsigned>(slot), static_cast<unsigned>(slots[slot].samples),
                      queuedAheadUs / 1000.0, lateUs / 1000.0);
        slot ^= 1;
    }

    if (ok) {
        // Generation is complete, but playRaw() is asynchronous. Keep the
        // buffers and speaker alive until the final queued segment and the DMA
        // tail have both finished.
        const int64_t timeout = std::max(esp_timer_get_time(), nextPlaybackStartUs) + 5000000;
        ok = waitForQueueBelow(1, timeout);
        if (ok) {
            const auto speakerCfg = M5.Speaker.config();
            const uint32_t drain =
                (speakerCfg.dma_buf_len * speakerCfg.dma_buf_count * 1000 +
                 kPlaybackSampleRate - 1) / kPlaybackSampleRate + 20;
            vTaskDelay(pdMS_TO_TICKS(drain));
        }
    }
    if (!ok && M5.Speaker.isRunning()) M5.Speaker.end();
    for (size_t i = 0; i < 2; ++i) releaseSlot(i);
#else
    size_t start = 0;
    while (ok && start < text.length()) {
        const size_t end = nextSegmentEnd(text, start, kMaxTextBytes);
        if (end <= start) {
            Serial.println("SanoTTS: invalid UTF-8 input");
            ok = false;
            break;
        }
        String segment = text.substring(start, end);
        segment.trim();
        if (segment.length()) ok = speakSegment(segment);
        start = end;
    }
#endif
    if (M5.Speaker.isRunning()) M5.Speaker.end();
    M5.Speaker.config(previousSpeakerConfig);
    if (speakerWasRunning && !M5.Speaker.begin()) {
        Serial.println("SanoTTS: failed to restore speaker");
    }
    if (micWasRunning) M5.Mic.begin();
    if (!ok) Serial.println("SanoTTS: synthesis/playback failed");
    return ok;
}

#if !defined(SANOTTS_BUFFERED_PLAYBACK)
bool SanoTTS::speakSegment(const String& text) {
    int32_t ids[kMaxIds];
    int32_t count = 0;
    int tokens = 0;
    const saan_kanji_status parsed = saan_kanji_to_ids(
        &g_dictionary, text.c_str(), text.length(), g_arenaMemory, kArenaBytes,
        ids, kMaxIds, &count, &tokens);
    if (parsed == SAAN_KANJI_OK && count > 0) {
        Serial.printf("SanoTTS: segment bytes=%u ids=%d\n",
                      static_cast<unsigned>(text.length()), static_cast<int>(count));
        return inferAndPlay(ids, count);
    }
    if (parsed == SAAN_KANJI_ERR_TOO_LONG || parsed == SAAN_KANJI_ERR_IDS) {
        const size_t split = retrySplitPoint(text);
        if (split > 0 && split < text.length()) {
            String first = text.substring(0, split);
            String second = text.substring(split);
            first.trim();
            second.trim();
            Serial.printf("SanoTTS: retry split at %u bytes (%s)\n",
                          static_cast<unsigned>(split), saan_kanji_strerror(parsed));
            return (!first.length() || speakSegment(first)) &&
                   (!second.length() || speakSegment(second));
        }
    }
    Serial.printf("SanoTTS: parse failed: %s\n", saan_kanji_strerror(parsed));
    return false;
}

bool SanoTTS::inferAndPlay(const int32_t *ids, int32_t count) {
    saan_arena arena;
    saan_stream stream = {};
    saan_arena_init(&arena, g_arenaMemory, kArenaBytes);
    saan_status status = saan_stream_init(&stream, &g_weights, &arena, ids, count, SAAN_S_V);
    if (status != SAAN_OK || arena.failed ||
        arena.used != saan_stream_arena_used(count) || stream.n_frames <= 0) return false;
    const uint64_t expectedWide = static_cast<uint64_t>(stream.n_frames) * SAAN_HOP;
    if (expectedWide > static_cast<uint64_t>(SAAN_SR) * 30 ||
        expectedWide > SIZE_MAX / sizeof(int16_t)) return false;
    const size_t expected = static_cast<size_t>(expectedWide);
    StreamingOutput output;
    if (!output.allocate(expected)) return false;
    size_t samples = 0;
    int64_t maxPullUs = 0;
    bool ended = false;
    for (size_t i = 0; i <= (expected + kPcmSamples - 1) / kPcmSamples; ++i) {
        int32_t frames = 0;
        const int64_t pullStarted = esp_timer_get_time();
        status = saan_stream_pull(&stream, g_pcm, &frames);
        const int64_t pullUs = esp_timer_get_time() - pullStarted;
        if (pullUs > maxPullUs) maxPullUs = pullUs;
        if (status != SAAN_OK || arena.failed || frames < 0 || frames > SAAN_CHUNK) return false;
        if (frames == 0) { ended = true; break; }
        const size_t n = static_cast<size_t>(frames) * SAAN_HOP;
        if (samples + n > expected) return false;
        int16_t *dst = output.destination(n);
        if (!dst) return false;
        int peak = 0;
        for (size_t j = 0; j < n; ++j) {
            if (!std::isfinite(g_pcm[j])) return false;
            const float scaled = g_pcm[j] * 32767.0f;
            long v;
            if (scaled > 32767.0f) v = 32767;
            else if (scaled < -32768.0f) v = -32768;
            else v = lrintf(scaled);
            dst[j] = static_cast<int16_t>(v);
            const int magnitude = abs(static_cast<int>(dst[j]));
            if (magnitude > peak) peak = magnitude;
        }
        level_ = peak;
        if (!output.submit(n)) return false;
        samples += n;
        vTaskDelay(1);
    }
    const bool complete = ended && samples == expected && output.finish(expected);
    Serial.printf("SanoTTS: max_pull=%.2f ms queue_empty_events=%u\n",
                  maxPullUs / 1000.0, output.queueEmptyEvents());
    return complete;
}
#endif

#endif
