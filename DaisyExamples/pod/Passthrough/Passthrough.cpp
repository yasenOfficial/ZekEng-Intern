#include "daisy_pod.h"

using namespace daisy;

// ——— Audio callback (interleaved) ———————————————————
void AudioCallback(const float* in, float* out, size_t size)
{
    // size = total samples (2 per frame: L, R)
    for(size_t i = 0; i < size; i++)
    {
        out[i] = in[i];
    }
}

int main(void)
{
    // 1) Initialize the Pod’s audio hardware
    DaisyPod hw;
    hw.Init();

    // 2) Start streaming audio with our passthrough callback
    hw.StartAudio(AudioCallback);

    // 3) Keep running forever
    while(1) {}
}
