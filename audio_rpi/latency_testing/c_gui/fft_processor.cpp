/*
 * fft_processor.cpp - FFT computation matching Python's _compute_fft().
 *
 * Algorithm: iterative Cooley-Tukey radix-2 FFT (in-place), Blackman window,
 * 10*log10 magnitude, exponential smoothing.
 */

#include "fft_processor.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Construction ────────────────────────────────────────────────────────── */

FFTProcessor::FFTProcessor() {
    make_blackman_window();
}

void FFTProcessor::make_blackman_window() {
    window_.resize(N_FFT);
    for (int n = 0; n < N_FFT; ++n) {
        /* Blackman window: w(n) = 0.42 - 0.5*cos(2πn/N) + 0.08*cos(4πn/N) */
        window_[n] = 0.42f
                   - 0.50f * std::cos(2.0f * (float)M_PI * n / (N_FFT - 1))
                   + 0.08f * std::cos(4.0f * (float)M_PI * n / (N_FFT - 1));
    }
}

/* ── In-place iterative Cooley-Tukey FFT ─────────────────────────────────── */

void FFTProcessor::fft_inplace(std::vector<std::complex<float>>& x) {
    int n = (int)x.size();

    /* Bit-reversal permutation */
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    /* Butterfly passes */
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float)M_PI / (float)len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<float> u = x[i + j];
                std::complex<float> v = x[i + j + len / 2] * w;
                x[i + j]           = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void FFTProcessor::compute(const std::deque<float>& buffer,
                           std::vector<float>&       out_freqs,
                           std::vector<float>&       out_mags_db) {
    /* Build windowed input (zero-pad when buffer is short) */
    std::vector<std::complex<float>> x(N_FFT, {0.0f, 0.0f});

    int buf_len = (int)buffer.size();
    int start   = (buf_len >= N_FFT) ? (buf_len - N_FFT) : 0;

    float mean = 0.0f;
    int   cnt  = (buf_len >= N_FFT) ? N_FFT : buf_len;
    for (int i = 0; i < cnt; ++i)
        mean += buffer[start + i];
    mean /= (float)(cnt > 0 ? cnt : 1);

    float window_sum = 0.0f;
    for (float w : window_) window_sum += w;

    for (int i = 0; i < N_FFT; ++i) {
        float sample = (i < buf_len) ? (buffer[start + i] - mean) : 0.0f;
        x[i] = {sample * window_[i], 0.0f};
    }

    fft_inplace(x);

    /* Magnitude (one-sided), normalised, in dBFS */
    int half = N_FFT / 2 + 1;
    std::vector<float> mags(half);
    for (int i = 0; i < half; ++i) {
        float mag = std::abs(x[i]) * 2.0f / window_sum;
        mags[i]   = 20.0f * std::log10(mag + 1e-12f);
    }

    /* Exponential smoothing (matches Python: smoothed = 0.7*prev + 0.3*new) */
    if (prev_mags_db_.size() == (size_t)half) {
        for (int i = 0; i < half; ++i)
            mags[i] = (1.0f - SMOOTHING) * prev_mags_db_[i] + SMOOTHING * mags[i];
    }
    prev_mags_db_ = mags;

    /* Frequency axis */
    out_freqs.resize(half);
    for (int i = 0; i < half; ++i)
        out_freqs[i] = (float)i * SAMPLE_RATE / (float)N_FFT;

    out_mags_db = mags;
}
