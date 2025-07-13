#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"
#include "WavFile.h"

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

    // DIR     dir;
    // FILINFO fno;
    // if(f_opendir(&dir, "/") == FR_OK)
    // {
    //     while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    //     {
    //         hw.PrintLine("Found: '%s'", fno.fname);
    //         hw.PrintLine("---");
    //         daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    //     }
    //     f_closedir(&dir);
    // }

    // // Check impulse response file size
    // FRESULT stat_res = f_stat(IMPULSE_RESPONSE, &fno);
    // if(stat_res != FR_OK)
    // {
    //     hw.PrintLine("File stat failed!");
    //     return 1;
    // }
    // hw.PrintLine("Impulse Response file size: %lu", (unsigned long)fno.fsize);

    // uint8_t header[256];

    // if(header == NULL)
    // {
    //     hw.PrintLine("Failed to allocate memory for full_file!");
    //     return 1;
    // }

    // if(f_open(&SDFile, IMPULSE_RESPONSE, FA_READ) == FR_OK)
    // {
    //     UINT    bytesread_total = 0;
    //     FRESULT fr = f_read(&SDFile, header, sizeof(header), &bytesread_total);
    //     f_close(&SDFile);

    //     if(fr == FR_OK && bytesread_total == sizeof(header))
    //     {
    //         // Now parse header from full_file, not from inbuff!
    //         WavFile wav;
    //         if(wav.ParseHeader(header, sizeof(header)))
    //         {
    //             wav.PrintHeader();

    //             // Print data_offset and data_size from the header struct
    //             const WavFile::Header& h = wav.GetHeader();

          
          
    //         }
    //         else
    //         {
    //             hw.PrintLine("Not a valid WAV header!");
    //             daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    //         }
    //     }
    //     else
    //     {
    //         hw.PrintLine(
    //             "Can't Read full file, f_read returned: %d (read %lu/%lu bytes)",
    //             fr,
    //             (unsigned long)bytesread_total,
    //             (unsigned long)sizeof(header));
    //         daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    //     }
    // }
    // else
    // {
    //     hw.PrintLine("Can't Open file");
    //     daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    // }



    ReadAndPrintWavHeader(IMPULSE_RESPONSE);



    return 0;
}
