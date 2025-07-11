#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"

// #define IMPULSE_RESPONSE "test.txt"
#define IMPULSE_RESPONSE "out48.wav"

using namespace daisy;

static DaisySeed hw;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

int main(void)
{
    // Vars an buffs.
    static char inbuff[512];
    UINT bytesread = 0;
    int  fail      = 0;

    // Init hardware
    hw.Init();
    hw.StartLog(true);

    // Init SD Card
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd.Init(sd_cfg);

    // Links libdaisy i/o to fatfs driver.
    fsi.Init(FatFSInterface::Config::MEDIA_SD);

    // Mount SD Card
    FRESULT mntres = f_mount(&fsi.GetSDFileSystem(), "/", 1);

    // Print all files
    DIR dir;
    FILINFO fno;
    if(f_opendir(&dir, "/") == FR_OK)
    {
        while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0]){

            hw.PrintLine("Found: '%s'", fno.fname);
            // hw.PrintLine("File attr: %02X", fno.fattrib);
            hw.PrintLine("---");
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        }

        f_closedir(&dir);
    }

    // check impulse response fille size
    FRESULT stat_res = f_stat(IMPULSE_RESPONSE, &fno);
    if(stat_res == FR_OK)
        hw.PrintLine("Impulse Response file size: %lu", (unsigned long)fno.fsize);

    if(f_open(&SDFile, IMPULSE_RESPONSE, FA_READ) == FR_OK)
    {
        FRESULT fr = f_read(&SDFile, inbuff, sizeof(inbuff), &bytesread);
        f_close(&SDFile);

        // Проверка: bytesread > 0 и fr == FR_OK
        if(fr == FR_OK && bytesread > 0)
        {
                   
            hw.PrintLine("f_read == FR_OK");
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

            // for(;;)
            // {
            //     hw.SetLed(true);
            //     System::Delay(250);
            //     hw.SetLed(false);
            //     System::Delay(250);
            //         hw.PrintLine("WOHOOO!");

            //         daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            // }

            uint32_t to_print = bytesread < 32 ? bytesread : 32;
            for (uint32_t i = 0; i < to_print; i++)
            {
                hw.PrintLine("Byte[%02lu]: 0x%02X (%d)", i, (uint8_t)inbuff[i], (int8_t)inbuff[i]);
            }
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        }
        else
        {
            hw.PrintLine("Can't Read file, f_read returned: %d", fr);
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            fail = 1;
        }
    }
    else
    {
        hw.PrintLine("Can't Open file");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        fail = 2;
    }

    // не е успяло: бавен мигащ LED
    for(;;)
    {
        for(int i = 0; i < fail + 1; i++)
        {
            hw.SetLed(true);
            System::Delay(1000);
            hw.SetLed(false);
            System::Delay(1000);
        }
    }
}
