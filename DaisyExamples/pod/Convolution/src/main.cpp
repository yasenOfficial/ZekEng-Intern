#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "WavFile.h"

#define IMPULSE_RESPONSE "out48.wav"

using namespace daisy;

static DaisySeed hw;
SdmmcHandler     sd;
FatFSInterface   fsi;
FIL              SDFile;

int main(void)
{
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
    if(stat_res != FR_OK)
    {
        hw.PrintLine("File stat failed!");
        return 1;
    }
    hw.PrintLine("Impulse Response file size: %lu", (unsigned long)fno.fsize);

    UINT filesize = fno.fsize;
    uint8_t* full_file = (uint8_t*)malloc(filesize);

    if(full_file == NULL)
    {
        hw.PrintLine("Failed to allocate memory for full_file!");
        return 1;
    }

    if(f_open(&SDFile, IMPULSE_RESPONSE, FA_READ) == FR_OK)
    {
        UINT    bytesread_total = 0;
        FRESULT fr = f_read(&SDFile, full_file, filesize, &bytesread_total);
        f_close(&SDFile);

        if(fr == FR_OK && bytesread_total == filesize)
        {
            // Now parse header from full_file, not from inbuff!
            WavFile wav;
            if(wav.ParseHeader(full_file, filesize))
            {
                wav.PrintHeader();

                // Print data_offset and data_size from the header struct
                const WavFile::Header& h = wav.GetHeader();

                // Check for possible overflow!
                if((unsigned long)h.data_offset + (unsigned long)h.data_size > (unsigned long)filesize)
                {
                    hw.PrintLine("ERROR: data_offset + data_size > filesize! WAV may be corrupt.");
                    daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
                }
                else
                {
                    wav.WriteAsNewFile("rebuilt.wav", full_file);
                }
            }
            else
            {
                hw.PrintLine("Not a valid WAV header!");
                daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            }
        }
        else
        {
            hw.PrintLine(
                "Can't Read full file, f_read returned: %d (read %lu/%lu bytes)",
                fr,
                (unsigned long)bytesread_total,
                (unsigned long)filesize);
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        }
        free(full_file);
    }
    else
    {
        hw.PrintLine("Can't Open file");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    }
}
