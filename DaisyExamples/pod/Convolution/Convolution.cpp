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
static FIR<MAX_IR, BLOCK> fir;
static float              irBuf[MAX_IR];


SdmmcHandler       sd;
FatFSInterface     fsi;
FIL     SDFile;

struct WavLoader
{
    static size_t Load(const char* path, float* dst, size_t max)
    {
        UINT    bytesread = 0;
        char    buf[512];

        // 1. Print all root files for debug
        DIR dir;
        FILINFO fno;
        if(f_opendir(&dir, "/") == FR_OK)
        {
            while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
                hw.PrintLine("Found: %s", fno.fname);
            f_closedir(&dir);
        }

        // 2. Get file info
        if(f_stat(path, &fno) == FR_OK)
            hw.PrintLine("File size: %lu", (unsigned long)fno.fsize);
        else
            hw.PrintLine("f_stat failed for %s", path);

        // 3. Open file
        FRESULT fres = f_open(&SDFile, path, FA_READ);
        if(fres != FR_OK)
        {
            hw.PrintLine("f_open failed: %d", fres);
            return 0;
        }
        else
        {
            hw.PrintLine("f_open succeeded for %s", path);
        }

        // 4. Read 512 bytes from the file
        bytesread = 0;
        FRESULT fr = f_read(&SDFile, buf, sizeof(buf), &bytesread);
        f_close(&SDFile);

        if(fr != FR_OK)
        {
            hw.PrintLine("f_read error: %d", fr);
            return 0;
        }
        hw.PrintLine("Tried to read 256, got %u bytes", bytesread);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

        for(UINT i = 0; i < bytesread; ++i)
            hw.PrintLine("Byte %u: 0x%02X", i, buf[i]);

        // 5. Close file
        f_close(&SDFile);

        // ✅ If we got bytes, return a “success” (simulate as 1 tap loaded, for now)
        return 0; // (bytesread > 0) ? 1 : 0;
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
    hw.StartLog(true);
    hw.PrintLine("Daisy Seed initialized");
    daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

    // 2) Mount SD + FATFS
    {
        SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd.Init(sd_cfg);
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        FRESULT res = f_mount(&fsi.GetSDFileSystem(), "/", 1);
        hw.PrintLine("f_mount returned %d\n", res);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
         UINT    bytesread = 0;
        char    buf[512];

        // 1. Print all root files for debug
        DIR dir;
        FILINFO fno;
        if(f_opendir(&dir, "/") == FR_OK)
        {
            while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
                hw.PrintLine("Found: %s", fno.fname);
            f_closedir(&dir);
        }

        // 2. Get file info
        if(f_stat("/out48.wav", &fno) == FR_OK)
            hw.PrintLine("File size: %lu", (unsigned long)fno.fsize);
        else
            hw.PrintLine("f_stat failed for %s", "/out48.wav");

        // 3. Open file
        FRESULT fres = f_open(&SDFile, "/out48.wav", FA_READ);
        if(fres != FR_OK)
        {
            hw.PrintLine("f_open failed: %d", fres);
            return 0;
        }
        else
        {
            hw.PrintLine("f_open succeeded for %s", "/out48.wav");
        }

        // 4. Read 512 bytes from the file
        bytesread = 0;
        FRESULT fr = f_read(&SDFile, buf, sizeof(buf), &bytesread);
        f_close(&SDFile);

        if(fr != FR_OK)
        {
            hw.PrintLine("f_read error: %d", fr);
            return 0;
        }
        hw.PrintLine("Tried to read 256, got %u bytes", bytesread);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

        for(UINT i = 0; i < bytesread; ++i)
            hw.PrintLine("Byte %u: 0x%02X", i, buf[i]);

        // 5. Close file
        f_close(&SDFile);

           hw.PrintLine("==============");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    }

    // 3) Load mono IR (from root, e.g., "/out48.wav")
    size_t len = WavLoader::Load("/out48.wav", irBuf, MAX_IR);
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
