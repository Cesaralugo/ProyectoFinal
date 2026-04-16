#pragma once
/*
 * fft_processor.hpp - Real-time FFT with Blackman window and exponential
 *                     smoothing, matching the Python GUI's _compute_fft().
 *
 * Usage:
 *   FFTProcessor fft;
 *   std::vector<float> freqs, mags_db;
 *   fft.compute(sample_buffer, freqs, mags_db);
 */

#include <vector>
#include <complex>
#include <deque>

class FFTProcessor {
public:
    static constexpr int   N_FFT       = 2048;
    static constexpr float SAMPLE_RATE = 44100.0f;
    static constexpr float SMOOTHING   = 0.3f;   /* α – matches Python's α=0.3 */

    FFTProcessor();

    /*
     * Compute FFT of the most recent N_FFT samples from `buffer`.
     * Outputs frequency axis (Hz) and smoothed magnitude spectrum (dBFS).
     * Matches Python's _compute_fft() exactly.
     */
    void compute(const std::deque<float>& buffer,
                 std::vector<float>& out_freqs,
                 std::vector<float>& out_mags_db);

private:
    std::vector<float>                prev_mags_db_;
    std::vector<float>                window_;        /* Blackman window */

    void make_blackman_window();
    void fft_inplace(std::vector<std::complex<float>>& x);
};
