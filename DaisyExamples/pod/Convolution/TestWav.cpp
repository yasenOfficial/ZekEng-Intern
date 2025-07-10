#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "fatfs.h"

#define TEST_FILE_NAME "out48.wav" 

using namespace daisy;

static DaisySeed hw;

SdmmcHandler   sd;
FatFSInterface fsi;
FIL            SDFile;

int main(void)
{
    // Vars an buffs.
    char   inbuff[512];
    UINT   bytesread = 0;
    int    fail      = 0;

    // Init hardware
    hw.Init();

    // Init SD Card
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd.Init(sd_cfg);

    // Links libdaisy i/o to fatfs driver.
    fsi.Init(FatFSInterface::Config::MEDIA_SD);

    // Mount SD Card
    FRESULT mntres = f_mount(&fsi.GetSDFileSystem(), "/", 1);

    // Чети WAV файла
    if(f_open(&SDFile, TEST_FILE_NAME, FA_READ) == FR_OK)
    {
        FRESULT fr = f_read(&SDFile, inbuff, sizeof(inbuff), &bytesread);
        f_close(&SDFile);

        // Проверка: bytesread > 0 и fr == FR_OK
        if(fr == FR_OK && bytesread > 0)
        {
            // Мигащ LED - успяхме!
            for(;;)
            {
                hw.SetLed(true);
                System::Delay(250);
                hw.SetLed(false);
                System::Delay(250);
            }
        }
        else
        {
            fail = 1;
        }
    }
    else
    {
        // Не може да отвори файла
        fail = 2;
    }

    // не е успяло: бавен мигащ LED
    for(;;)
    {
        for(int i = 0; i < fail + 1; i++)
        {
            hw.SetLed(true);
            System::Delay(100);
            hw.SetLed(false);
            System::Delay(100);
        }
        System::Delay(1000);
    }
}
