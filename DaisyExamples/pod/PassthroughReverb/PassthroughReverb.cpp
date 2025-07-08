#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod   hw;
ReverbSc   reverb;

// Enable/disable the reverb effect
bool       reverbOn     = true;

// State for smoothing between wet/dry transitions
static float currentWet = 0.f;  // starts fully dry

// ——— Audio callback (called in interrupt context) —————
// 'in'  : pointer to input samples [L, R, L, R, ...]
// 'out' : pointer to output samples
// 'size': number of floats in 'in'/'out' buffers (always even)
void AudioCallback(const float* in, float* out, size_t size)
{

    // This function has to execute fast, because its an interrupt and it has to be realtime (< 1ms)
    // Wet mix: 0.5 (50% reverb) when on, 0 when off - toggled by the button
    const float targetWet = reverbOn ? 0.5f : 0.f;
    
    //  ~0.002 = ~10–15 ms fade - 48 kHz
    currentWet += (targetWet - currentWet) * 0.002f;
    
    // wet/dry mix values for this buffer
    const float wetMix = currentWet;
    const float dryMix = 1.f - wetMix;

    // Process each stereo sample pair
    for(size_t i = 0; i < size; i += 2)
    {
        // Read left/right input
        float inL = in[i];
        float inR = in[i + 1];
        
        float wetL, wetR;

        // Always pass the input through the reverb engine
        reverb.Process(inL, inR, &wetL, &wetR);

        // Mix dry (original) and wet (processed) signals
        // if wetMix = 0, output = input
        out[i]   = inL * dryMix + wetL * wetMix;
        out[i+1] = inR * dryMix + wetR * wetMix;
    }
}

int main(void)
{
    // 1) Initialize Pod hardware (audio, buttons, LEDs)
    hw.Init();

    // 2) Configure the reverb effect

    //    - Use the same sample rate as the audio engine
    reverb.Init(hw.AudioSampleRate());

    //    - Feedback controls the decay length (0.0 -> short, 0.99 -> long)
    reverb.SetFeedback(0.8f);

    //    - cut off frequencies above 6 kHz
    reverb.SetLpFreq(6000.f);


    // 3) Start the audio engine with our callback
    hw.StartAudio(AudioCallback);

    // 4) Main loop: poll buttons and toggle reverb state
    while(1)
    {
        // Update button states
        hw.ProcessDigitalControls();

        // On button1 press, toogle reverbOn flag
        if(hw.button1.RisingEdge())
            reverbOn = !reverbOn;
    }
}
