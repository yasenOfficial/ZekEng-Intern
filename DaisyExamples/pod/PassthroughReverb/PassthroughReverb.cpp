#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// ——— Hardware & DSP objects —————————————————————
DaisyPod   hw;
ReverbSc   reverb;
bool       reverbOn     = true;

// ——— Audio callback (interleaved) ———————————————————
void AudioCallback(const float* in, float* out, size_t size)
{
    const float dryMix = 0.5f, wetMix = 0.5f;

    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i], inR = in[i + 1];
        if(reverbOn)
        {
            float wetL, wetR;
            reverb.Process(inL, inR, &wetL, &wetR);
            out[i]     = inL  * dryMix + wetL  * wetMix;
            out[i+1]   = inR  * dryMix + wetR  * wetMix;
        }
        else
        {
            // bypass
            out[i]     = inL;
            out[i+1]   = inR;
        }
    }
}

int main(void)
{
    // 1) Initialize Pod hardware (audio, controls, LEDs, etc.)
    hw.Init();                                                              // :contentReference[oaicite:0]{index=0}

    // 2) Init the reverb at the current sample rate
    reverb.Init(hw.AudioSampleRate());
    reverb.SetFeedback(0.8f);
    reverb.SetLpFreq(6000.f);

    // 3) Start the audio callback
    hw.StartAudio(AudioCallback);

    // 4) In the main loop, poll the button and toggle reverb
    while(1)
    {
        // Updates hw.button1 & hw.button2 states
        hw.ProcessDigitalControls();                                       // :contentReference[oaicite:1]{index=1}

        // On a rising edge of BUTTON_1, flip reverbOn
        if(hw.button1.RisingEdge())
            reverbOn = !reverbOn;
    }
}