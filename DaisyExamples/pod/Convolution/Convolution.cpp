// Convolution.cpp – Daisy Seed: live audio ► mono IR ► stereo out
#include "daisy_seed.h"
#include "daisysp.h"
#include "fatfs.h"

using namespace daisy;
using namespace daisysp;

/* config */
constexpr size_t BLOCK  = 48;      // 1 ms @ 48 kHz
constexpr size_t MAX_IR = 1024;    // taps buffer size

/* globals */
static DaisySeed          hw;
static SdmmcHandler       sd;
static FatFSInterface     fsi;
static FIR<MAX_IR, BLOCK> fir;
static float              irBuf[MAX_IR];

/* Tiny WAV loader for the IR */
struct WavLoader
{
    static size_t Load(const char* path, float* dst, size_t max)
    {
        hw.PrintLine("WavLoader: opening %s\n", path);
        FIL f; UINT br;
        if(f_open(&f, path, FA_READ) != FR_OK)
        {
            hw.PrintLine("WavLoader: open failed");
            return 0;
        }
        // Read header
        f_lseek(&f, 22); uint16_t ch;   f_read(&f, &ch,   2, &br);
        f_lseek(&f, 24); uint32_t sr;   f_read(&f, &sr,   4, &br);
        f_lseek(&f, 34); uint16_t bits; f_read(&f, &bits, 2, &br);

        hw.PrintLine("WavLoader: bad fmt ch=%d \n", 2);
        hw.PrintLine("WavLoader: bad fmt sr=%d \n", 1);

        if(ch != 1)
        {
            hw.PrintLine("WavLoader: bad fmt ch=%d \n", int(ch));
            f_close(&f);
            return 0;
        }
        if(sr != 48000)
        {
            hw.PrintLine("WavLoader: bad fmt sr=%d \n", int(sr));
            f_close(&f);
            return 0;
        }

        // Find data chunk
        f_lseek(&f, 36);
        uint32_t id, size;
        while(true)
        {
            f_read(&f, &id,   4, &br);
            f_read(&f, &size, 4, &br);
            if(id == 0x61746164) break;  // "data"
            f_lseek(&f, f_tell(&f) + size);
        }
        size_t samples = size / (bits/8);
        if(samples > max) samples = max;
        hw.PrintLine("WavLoader: data size=%u samp=%u bits=%u\n",
                  (unsigned)size, (unsigned)samples, (unsigned)bits);
        // Read & convert
        if(bits == 16)
        {
            for(size_t i = 0; i < samples; i++)
            {
                int16_t v; f_read(&f, &v, 2, &br);
                dst[i] = s162f(v);
            }
        }
        else if(bits == 24)
        {
            uint8_t b[3];
            for(size_t i = 0; i < samples; i++)
            {
                f_read(&f, b, 3, &br);
                int32_t v = (int32_t(b[2])<<16)|(b[1]<<8)|b[0];
                if(v & 0x800000) v |= 0xFF000000;
                dst[i] = v / 8388608.f;
            }
        }
        else // 32-bit
        {
            for(size_t i = 0; i < samples; i++)
            {
                int32_t v; f_read(&f, &v, 4, &br);
                dst[i] = v / 2147483648.f;
            }
        }
        f_close(&f);
        hw.PrintLine("WavLoader: loaded %u taps\n", (unsigned)samples);
        return samples;
    }
};

/* Audio callback: left-in ► FIR ► stereo out */
void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        float wet = fir.Process(in[i]);
        out[i]   = wet;
        out[i+1] = wet;
    }
}

int main()
{
    // 1) Init hardware + USB log
    hw.Init();
    hw.StartLog();  
    hw.PrintLine("Daisy Seed initialized");

    // 2) Mount SD + FATFS
    {
        SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd.Init(sd_cfg);
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        FRESULT res = f_mount(&fsi.GetSDFileSystem(), "/", 1);
        hw.PrintLine("f_mount returned %d\n", res);
    }

    // 3) Load mono IR
    size_t len = WavLoader::Load("/IR/out48.wav", irBuf, MAX_IR);
    if(len == 0)
    {
        hw.PrintLine("Error: IR load failed, halting");
        while(1) {}
    }
    fir.SetIR(irBuf, len, true);
    hw.PrintLine("FIR configured: %u taps\n", (unsigned)len);

    // 4) Start audio engine
    hw.SetAudioBlockSize(BLOCK);
    hw.StartAudio(AudioCallback);
    hw.PrintLine("Audio started, block=%u\n", (unsigned)BLOCK);

    // 5) Idle loop
    while(1)
        hw.DelayMs(10);
}