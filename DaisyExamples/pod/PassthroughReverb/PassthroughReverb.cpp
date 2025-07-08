#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// ——— Hardware & DSP objects —————————————————————
DaisyPod   hw;
ReverbSc   reverb;
bool       reverbOn     = true;

static float currentWet = 0.f; 

void AudioCallback(const float* in, float* out, size_t size)
{
    // targetWet jumps between 0 (bypass) and 0.5 (full reverb)
    const float targetWet = reverbOn ? 0.5f : 0.f;
    currentWet += (targetWet - currentWet) * 0.002f;
    const float wetMix = currentWet;
    const float dryMix = 1.f - wetMix;

   for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i], inR = in[i+1];
        float wetL, wetR;

        reverb.Process(inL, inR, &wetL, &wetR);

        // wetMix ще е 0 при bypass
        out[i]   = inL * (1.f - wetMix) + wetL * wetMix;
        out[i+1] = inR * (1.f - wetMix) + wetR * wetMix;
    }
}


int main(void)
{
    hw.Init();                                                              

    reverb.Init(hw.AudioSampleRate());
    reverb.SetFeedback(0.8f);
    reverb.SetLpFreq(6000.f);

    hw.StartAudio(AudioCallback);

    while(1)
    {
        hw.ProcessDigitalControls();                                      
        if(hw.button1.RisingEdge())
            reverbOn = !reverbOn;
    }
}