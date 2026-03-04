#include <iostream>
#include <vector>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 44100
#define FRAMES 256

int main() {
    snd_pcm_t* handle;
    snd_pcm_hw_params_t* params;
    int dir;
    std::vector<int16_t> buffer(FRAMES);

    int rc = snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        std::cerr << "Error al abrir el dispositivo: " << snd_strerror(rc) << std::endl;
        return 1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);

    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 1);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        std::cerr << "Error al configurar parámetros HW: " << snd_strerror(rc) << std::endl;
        return 1;
    }

    std::cout << "Capturando audio..." << std::endl;

    while (true) {
        rc = snd_pcm_readi(handle, buffer.data(), FRAMES);
        if (rc == -EPIPE) {
            // buffer overrun
            snd_pcm_prepare(handle);
            std::cerr << "Overrun! Re-preparando PCM..." << std::endl;
        } else if (rc < 0) {
            std::cerr << "Error leyendo audio: " << snd_strerror(rc) << std::endl;
        } else {
            std::cout << "Primer sample: " << buffer[0] << std::endl;
        }
    }

    snd_pcm_close(handle);
    return 0;
}