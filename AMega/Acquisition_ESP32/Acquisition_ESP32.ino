/*
 * Acquisition_ESP32.ino
 * ═════════════════════════════════════════════════════════════════════════════
 * Real-time ADC sampler for ESP32-WROOM-32
 * Target core: Arduino ESP32 core 1.0.4
 *
 * Samples GPIO36 (ADC1_CH0) at ~44 kHz via hardware timer interrupt,
 * packs 512 samples per packet, and streams them over UART at 921600 baud.
 *
 * Packet wire format (matches monitor.py / diagnose_esp32.py):
 *   [0xAA 0x55 0xFF 0x00]  ← 4-byte sync word
 *   [512 × uint16 LE]      ← 1024 payload bytes, raw 12-bit ADC codes (0–4095)
 *
 * Total packet size: 1028 bytes
 * Throughput:  1028 B × (44077/512) ≈ 88.5 kB/s  << 921600 baud ≈ 115 kB/s ✓
 *
 * Timer arithmetic (prescaler 80 → 1 tick = 1 µs @ 80 MHz APB):
 *   Period = 1 000 000 / 44077 ≈ 22.69 µs → alarm count = 23
 *   Actual Fs = 1 000 000 / 23 ≈ 43 478 Hz  (close enough for audio work)
 *
 * Double-buffer scheme:
 *   ISR fills buf[fill_idx][].
 *   When a buffer is full the ISR toggles fill_idx and sets ready_idx + flag.
 *   loop() detects the flag, writes the completed buffer to Serial, clears flag.
 *   This keeps the ISR short and avoids any Serial call inside the ISR.
 *
 * Wiring:
 *   Signal → GPIO36 (VP / ADC1_CH0)  via ≤100 Ω series resistor
 *   GND    → ESP32 GND
 *   Do NOT exceed 3.3 V on GPIO36.
 *
 * Python monitor usage:
 *   python monitor.py --port /dev/ttyUSB0 --baud 921600 \
 *                     --fs 43478 --vref 3.3 --esp32
 *
 * Diagnostic:
 *   python diagnose_esp32.py --port /dev/ttyUSB0 --baud 921600
 * ═════════════════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>

// ── User-adjustable parameters ────────────────────────────────────────────────
#define ADC_PIN        36          // GPIO36 = ADC1_CH0 (VP pin)
#define PACKET_SAMPLES 512         // samples per packet; must match monitor.py BUFFER_SIZE
#define SERIAL_BAUD    921600      // 921600 baud — reliable on CH340

// Timer: prescaler 80 → 1 tick = 1 µs.  Period = 23 µs → Fs ≈ 43 478 Hz.
// Adjust TIMER_PERIOD_US to trim the actual sample rate if needed.
#define TIMER_PERIOD_US 23

// ── Sync word ─────────────────────────────────────────────────────────────────
static const uint8_t SYNC[4] = { 0xAA, 0x55, 0xFF, 0x00 };

// ── Double-buffer ─────────────────────────────────────────────────────────────
// Two buffers: one being filled by ISR, one being drained by loop().
// Declared volatile because the ISR writes to them asynchronously.
static volatile uint16_t buf[2][PACKET_SAMPLES];

// Which buffer the ISR is currently filling (0 or 1).
static volatile uint8_t  fill_idx  = 0;

// Which buffer loop() should send next (set by ISR when a buffer completes).
static volatile uint8_t  ready_idx = 1;

// Flag: true when a complete buffer is waiting to be sent.
static volatile bool     buf_ready = false;

// Current write position inside buf[fill_idx][].
static volatile int      pos       = 0;

// ── Transmit scratch buffer (avoids Serial.write() inside ISR) ───────────────
// 4 sync bytes + 512 × 2 payload bytes = 1028 bytes
#define TX_BUF_SIZE  (4 + PACKET_SAMPLES * 2)
static uint8_t tx_buf[TX_BUF_SIZE];

// ── Hardware timer handle ─────────────────────────────────────────────────────
static hw_timer_t *sample_timer = NULL;

// ── ISR ───────────────────────────────────────────────────────────────────────
/*
 * Runs every TIMER_PERIOD_US microseconds.
 * Reads the ADC, stores the 12-bit result into the active buffer.
 * When the buffer is full it swaps buffers atomically and signals loop().
 *
 * analogRead() on ESP32 core 1.0.4 takes ≈ 10–15 µs for a 12-bit conversion,
 * which fits comfortably inside a 23 µs ISR period.
 *
 * IRAM_ATTR ensures the ISR is placed in IRAM so it can run even during
 * flash cache misses (e.g. while loop() is writing to Serial).
 */
void IRAM_ATTR onSampleTimer()
{
    uint16_t raw = (uint16_t)analogRead(ADC_PIN);
    buf[fill_idx][pos] = raw;
    pos++;

    if (pos >= PACKET_SAMPLES) {
        pos = 0;
        // Signal that fill_idx buffer is complete.
        ready_idx = fill_idx;
        buf_ready = true;
        // Swap to the other buffer.
        fill_idx ^= 1;
    }
}

// ── setup() ───────────────────────────────────────────────────────────────────
void setup()
{
    // ── Serial ────────────────────────────────────────────────────────────────
    Serial.begin(SERIAL_BAUD);

    // ── ADC configuration ─────────────────────────────────────────────────────
    // 12-bit resolution → codes 0–4095.
    analogReadResolution(12);

    // ADC_11db attenuation → full-scale ≈ 3.3 V (actually 3.9 V, but the
    // ESP32 ADC is non-linear above ~3.1 V; keep input ≤ 3.3 V).
    analogSetAttenuation(ADC_11db);

    // Restrict sampling to ADC1 (ADC2 is shared with Wi-Fi).
    // analogSetPinAttenuation() works on core 1.0.4.
    analogSetPinAttenuation(ADC_PIN, ADC_11db);

    // Warm-up: discard first few reads (ADC needs a few cycles to settle).
    for (int i = 0; i < 16; i++) {
        analogRead(ADC_PIN);
    }

    // ── Hardware timer ────────────────────────────────────────────────────────
    // Timer 0, prescaler 80 → 1 tick per µs at 80 MHz APB clock.
    sample_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(sample_timer, &onSampleTimer, true);
    timerAlarmWrite(sample_timer, TIMER_PERIOD_US, true);
    timerAlarmEnable(sample_timer);
}

// ── loop() ────────────────────────────────────────────────────────────────────
/*
 * When a buffer is ready:
 *  1. Snapshot the ready buffer index and clear the flag (atomic).
 *  2. Build the full packet in tx_buf (sync + payload).
 *  3. Write tx_buf to Serial in one call for maximum throughput.
 *
 * Using a local copy of ready_idx before clearing buf_ready avoids a race
 * where the ISR sets a new ready_idx while we are still building the packet.
 *
 * Serial.write(buf, len) is non-blocking on ESP32 (uses a HW FIFO + DMA);
 * the 1028-byte packet at 921600 baud takes ≈ 11 ms to shift out, well within
 * the 11.8 ms it takes the ISR to refill the next buffer at 43 kHz.
 */
void loop()
{
    if (!buf_ready) {
        return;   // nothing to do; ISR is still filling
    }

    // Snapshot which buffer completed.
    uint8_t idx = ready_idx;

    // Clear the flag BEFORE we start copying so we don't block the ISR.
    buf_ready = false;

    // ── Build packet ──────────────────────────────────────────────────────────
    // Copy sync word.
    tx_buf[0] = SYNC[0];
    tx_buf[1] = SYNC[1];
    tx_buf[2] = SYNC[2];
    tx_buf[3] = SYNC[3];

    // Copy samples as little-endian uint16.
    // volatile pointer cast: safe because we own this buffer until next swap.
    const volatile uint16_t *src = buf[idx];
    uint8_t *dst = tx_buf + 4;
    for (int i = 0; i < PACKET_SAMPLES; i++) {
        uint16_t s = src[i];          // read volatile word once
        *dst++ = (uint8_t)(s & 0xFF); // low byte
        *dst++ = (uint8_t)(s >> 8);   // high byte
    }

    // ── Transmit ──────────────────────────────────────────────────────────────
    Serial.write(tx_buf, TX_BUF_SIZE);
}
