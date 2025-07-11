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

class WavFile
{
  public:
    struct Header
    {
        char     riff_id[5];
        uint32_t file_size;
        char     wave_id[5];
        char     fmt_id[5];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        char     data_id[5];
        uint32_t data_size;
    };

    WavFile() {}

    // Parses the first 44 bytes (standard PCM header)
    bool ParseHeader(const uint8_t* buf)
    {
        memcpy(header.riff_id, buf, 4);
        header.riff_id[4] = '\0';
        header.file_size
            = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
        memcpy(header.wave_id, buf + 8, 4);
        header.wave_id[4] = '\0';
        memcpy(header.fmt_id, buf + 12, 4);
        header.fmt_id[4] = '\0';
        header.fmt_size
            = buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24);
        header.audio_format = buf[20] | (buf[21] << 8);
        header.num_channels = buf[22] | (buf[23] << 8);
        header.sample_rate
            = buf[24] | (buf[25] << 8) | (buf[26] << 16) | (buf[27] << 24);
        header.byte_rate
            = buf[28] | (buf[29] << 8) | (buf[30] << 16) | (buf[31] << 24);
        header.block_align     = buf[32] | (buf[33] << 8);
        header.bits_per_sample = buf[34] | (buf[35] << 8);
        memcpy(header.data_id, buf + 36, 4);
        header.data_id[4] = '\0';
        header.data_size
            = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);

        // Minimal validation
        hw.PrintLine("riff_id: '%s' (expected: 'RIFF')", header.riff_id);
        hw.PrintLine("wave_id: '%s' (expected: 'WAVE')", header.wave_id);
        hw.PrintLine("fmt_id : '%s' (expected: 'fmt ') [note the space!]", header.fmt_id);
        hw.PrintLine("data_id: '%s' (expected: 'data')", header.data_id);

        hw.PrintLine("strcmp(riff_id, 'RIFF') = %d", strcmp(header.riff_id, "RIFF"));
        hw.PrintLine("strcmp(wave_id, 'WAVE') = %d", strcmp(header.wave_id, "WAVE"));
        hw.PrintLine("strcmp(fmt_id , 'fmt ') = %d", strcmp(header.fmt_id, "fmt "));
        hw.PrintLine("strcmp(data_id, 'data') = %d", strcmp(header.data_id, "data"));
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();



        return  1;
    }

    const Header& GetHeader() const { return header; }

    void PrintHeader()
    {
        hw.PrintLine("RIFF: %s", header.riff_id);
        hw.PrintLine("File size: %lu", (unsigned long)header.file_size);
        hw.PrintLine("WAVE: %s", header.wave_id);
        hw.PrintLine("fmt : %s", header.fmt_id);
        hw.PrintLine("Audio Format: %u", header.audio_format);
        hw.PrintLine("Channels: %u", header.num_channels);
        hw.PrintLine("Sample Rate: %lu", (unsigned long)header.sample_rate);
        hw.PrintLine("Byte Rate: %lu", (unsigned long)header.byte_rate);
        hw.PrintLine("Block Align: %u", header.block_align);
        hw.PrintLine("Bits Per Sample: %u", header.bits_per_sample);
        hw.PrintLine("data: %s", header.data_id);
        hw.PrintLine("Data Size: %lu", (unsigned long)header.data_size);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
    }

  private:
    Header header;
};


int main(void)
{
    // Vars an buffs.
    static char inbuff[512];
    UINT        bytesread = 0;
    int         fail      = 0;

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
    DIR     dir;
    FILINFO fno;
    if(f_opendir(&dir, "/") == FR_OK)
    {
        while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
        {
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
        hw.PrintLine("Impulse Response file size: %lu",
                     (unsigned long)fno.fsize);

    if(f_open(&SDFile, IMPULSE_RESPONSE, FA_READ) == FR_OK)
    {
        FRESULT fr = f_read(&SDFile, inbuff, sizeof(inbuff), &bytesread);
        f_close(&SDFile);

        // Проверка: bytesread > 0 и fr == FR_OK
        if(fr == FR_OK && bytesread > 0)
        {
            hw.PrintLine("f_read == FR_OK");
            hw.PrintLine("Read bytes = %u", bytesread);

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
            for(uint32_t i = 0; i < to_print; i++)
            {
                hw.PrintLine("Byte[%02lu]: 0x%02X (%d)",
                             i,
                             (uint8_t)inbuff[i],
                             (int8_t)inbuff[i]);
            }
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

            WavFile wav;
            if(wav.ParseHeader((const uint8_t*)inbuff))
            {
                wav.PrintHeader();
            }
            else
            {
                hw.PrintLine("Not a valid PCM WAV header!");
                daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            }
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
