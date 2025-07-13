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

    // CopyWavFileChunked(IMPULSE_RESPONSE, "out.wav", 4096);

    ConvolveFiles("KriskoTest.wav", "impulse.wav", "out.wav", 1024);


    return 0;
}
