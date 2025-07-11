#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "WavFile.h"

#define IMPULSE_RESPONSE "out48.wav"

using namespace daisy;

static DaisySeed hw;
SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

int main(void)
{
    static char inbuff[512];
    UINT        bytesread = 0;

    // Init hardware
    hw.Init();
    hw.StartLog(true);

    // Init SD Card
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd.Init(sd_cfg);

    // Link libdaisy i/o to fatfs driver
    fsi.Init(FatFSInterface::Config::MEDIA_SD);

    // Mount SD Card
    FRESULT mntres = f_mount(&fsi.GetSDFileSystem(), "/", 1);

    // Print all files
    DIR     dir;
    FILINFO fno;
    if(f_opendir(&dir, "/") == FR_OK)
    {
        while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
        {
            hw.PrintLine("Found: '%s'", fno.fname);
            hw.PrintLine("---");
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        }
        f_closedir(&dir);
    }

    // Check impulse response file size
    FRESULT stat_res = f_stat(IMPULSE_RESPONSE, &fno);
    if(stat_res == FR_OK)
        hw.PrintLine("Impulse Response file size: %lu", (unsigned long)fno.fsize);

    if(f_open(&SDFile, IMPULSE_RESPONSE, FA_READ) == FR_OK)
    {
        FRESULT fr = f_read(&SDFile, inbuff, sizeof(inbuff), &bytesread);
        f_close(&SDFile);

        if(fr == FR_OK && bytesread > 0)
        {
            WavFile wav;
            if(wav.ParseHeader((const uint8_t*)inbuff))
            {
                wav.PrintHeader();
                // Here you would call your IR loading/convolution DSP setup, etc.
            }
            else
            {
                hw.PrintLine("Not a valid WAV header!");
                daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            }
        }
        else
        {
            hw.PrintLine("Can't Read file, f_read returned: %d", fr);
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        }
    }
    else
    {
        hw.PrintLine("Can't Open file");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    }

    // You can add a main loop or audio callback here for DSP processing!
}
