// Convolution.cpp – Daisy Seed: live audio ► mono IR ► stereo out
#include "daisy_seed.h"
#include "daisysp.h"
#include "fatfs.h"

using namespace daisy;
using namespace daisysp;

/* config */
constexpr size_t BLOCK  = 48;   // 1 ms @ 48 kHz
constexpr size_t MAX_IR = 1024; // taps buffer size

/* globals */
static DaisySeed          hw;
static SdmmcHandler       sd;
static FatFSInterface     fsi;
static FIR<MAX_IR, BLOCK> fir;
static float              irBuf[MAX_IR];

struct WavLoader
{
    static size_t Load(const char* path, float* dst, size_t max)
    {
        FIL     f;
        UINT    br;
        uint8_t header[12] = {0};
        DIR     dir;
        FILINFO fno;
        if(f_opendir(&dir, "/IR") == FR_OK)
        {
            while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
            {
                hw.PrintLine("File: %s", fno.fname);
            }
            f_closedir(&dir);
        }
        else
        {
            hw.PrintLine("Failed to open /IR");
        }


        if(f_open(&f, path, FA_READ) != FR_OK)
        {
            hw.PrintLine("Failed to open file!");
            return 0;
        }

        f_read(&f, header, 12, &br);
        hw.PrintLine("Read %d bytes", br);
        for(int i = 0; i < 12; i++)
        {
            hw.PrintLine("header[%d] = 0x%02X (%c)",
                         i,
                         header[i],
                         (header[i] >= 32 && header[i] < 127) ? header[i]
                                                              : '.');
        }
        f_close(&f);


        // if(riff != 0x46464952 || wave != 0x45564157) // "RIFF", "WAVE"
        // {
        //     hw.PrintLine("WavLoader: not a WAV");
        //     hw.DelayMs(1);
        //     f_close(&f);
        //     return 0;
        // }

        // 3) Scan for fmt chunk
        uint32_t chunkId, chunkSize;
        uint16_t audioFmt = 0, numCh = 0, bitsPerSample = 0;
        uint32_t sampleRate = 0;
        while(true)
        {
            f_read(&f, &chunkId, 4, &br);
            f_read(&f, &chunkSize, 4, &br);
            if(chunkId == 0x20746D66) // "fmt "
            {
                // read fields
                f_read(&f, &audioFmt, 2, &br);
                f_read(&f, &numCh, 2, &br);
                f_read(&f, &sampleRate, 4, &br);
                // skip rest of fmt chunk
                f_lseek(&f, f_tell(&f) + (chunkSize - 8));
                hw.PrintLine("fmt chunk: fmt=%u ch=%u sr=%u\n",
                             (unsigned)audioFmt,
                             (unsigned)numCh,
                             (unsigned)sampleRate);
                break;
            }
            // skip unknown chunk
            f_lseek(&f, f_tell(&f) + chunkSize);
        }

        // 4) Sanity check
        if(audioFmt != 1 || numCh != 1 || sampleRate != 48000)
        {
            hw.PrintLine("WavLoader: bad fmt fmt=%u ch=%u sr=%u\n",
                         (unsigned)audioFmt,
                         (unsigned)numCh,
                         (unsigned)sampleRate);
            f_close(&f);
            return 0;
        }

        // 5) Scan for data chunk
        size_t samples = 0;
        while(true)
        {
            f_read(&f, &chunkId, 4, &br);
            f_read(&f, &chunkSize, 4, &br);
            if(chunkId == 0x61746164) // "data"
            {
                samples = chunkSize / (bitsPerSample / 8);
                if(samples > max)
                    samples = max;
                hw.PrintLine("data chunk: size=%u samples=%u bits=%u\n",
                             (unsigned)chunkSize,
                             (unsigned)samples,
                             (unsigned)bitsPerSample);
                break;
            }
            // skip other chunks
            f_lseek(&f, f_tell(&f) + chunkSize);
        }

        // 6) Read & convert sample data
        if(bitsPerSample == 16)
        {
            for(size_t i = 0; i < samples; ++i)
            {
                int16_t v;
                f_read(&f, &v, 2, &br);
                dst[i] = s162f(v);
            }
        }
        else if(bitsPerSample == 24)
        {
            uint8_t buf[3];
            for(size_t i = 0; i < samples; ++i)
            {
                f_read(&f, buf, 3, &br);
                int32_t v = (int32_t(buf[2]) << 16) | (buf[1] << 8) | buf[0];
                if(v & 0x800000)
                    v |= 0xFF000000;
                dst[i] = v / 8388608.f;
            }
        }
        else // 32-bit
        {
            for(size_t i = 0; i < samples; ++i)
            {
                int32_t v;
                f_read(&f, &v, 4, &br);
                dst[i] = v / 2147483648.f;
            }
        }

        f_close(&f);
        hw.PrintLine("WavLoader: loaded %u taps\n", (unsigned)samples);
        return samples;
    }
};


/* Audio callback: left-in ► FIR ► stereo out */
void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        float wet  = fir.Process(in[i]);
        out[i]     = wet;
        out[i + 1] = wet;
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