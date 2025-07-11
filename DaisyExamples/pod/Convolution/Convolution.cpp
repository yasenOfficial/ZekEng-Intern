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
        // "fmt " chunk
        char     fmt_id[5];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        // "data" chunk
        char     data_id[5];
        uint32_t data_size;
        uint32_t data_offset; // offset in buffer/file where data starts
    };

    WavFile() {}

    /**
     * Parse the header, searching for "fmt " and "data" chunks anywhere in the first N bytes.
     * Returns true on valid WAV header (RIFF/WAVE, both chunks found).
     */
    bool ParseHeader(const uint8_t* buf, size_t bufsize = 512)
    {
        // Zero out
        memset(&header, 0, sizeof(header));
        header.data_offset = 0;

        // RIFF chunk at 0
        memcpy(header.riff_id, buf, 4); header.riff_id[4] = '\0';
        header.file_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
        memcpy(header.wave_id, buf + 8, 4); header.wave_id[4] = '\0';

        // is Wav?
        if(strcmp(header.riff_id, "RIFF") != 0 || strcmp(header.wave_id, "WAVE") != 0)
            return false;

        // Search for "fmt " and "data" chunks
        size_t offset = 12;
        bool found_fmt = false, found_data = false;

        while(offset + 8 <= bufsize)
        {
            char chunk_id[5];
            memcpy(chunk_id, buf + offset, 4); chunk_id[4] = '\0';
            uint32_t chunk_size = buf[offset+4] | (buf[offset+5]<<8) | (buf[offset+6]<<16) | (buf[offset+7]<<24);

            if(strcmp(chunk_id, "fmt ") == 0 && chunk_size >= 16)
            {
                found_fmt = true;
                memcpy(header.fmt_id, chunk_id, 5);
                header.fmt_size = chunk_size;
                header.audio_format = buf[offset+8] | (buf[offset+9]<<8);
                header.num_channels = buf[offset+10] | (buf[offset+11]<<8);
                header.sample_rate = buf[offset+12] | (buf[offset+13]<<8) | (buf[offset+14]<<16) | (buf[offset+15]<<24);
                header.byte_rate = buf[offset+16] | (buf[offset+17]<<8) | (buf[offset+18]<<16) | (buf[offset+19]<<24);
                header.block_align = buf[offset+20] | (buf[offset+21]<<8);
                header.bits_per_sample = buf[offset+22] | (buf[offset+23]<<8);
                // Note: skip extra format bytes if there are
            }
            else if(strcmp(chunk_id, "data") == 0)
            {
                found_data = true;
                memcpy(header.data_id, chunk_id, 5);
                header.data_size = chunk_size;
                header.data_offset = offset + 8; // data starts after chunk header
            }

            // Move to next chunk
            offset += 8 + ((chunk_size + 1) & ~1);
        }

        // Debug printing
        // hw.PrintLine("riff_id: '%s' (expected: 'RIFF')", header.riff_id);
        // hw.PrintLine("wave_id: '%s' (expected: 'WAVE')", header.wave_id);
        // hw.PrintLine("fmt_id : '%s' (expected: 'fmt ')", header.fmt_id);
        // hw.PrintLine("data_id: '%s' (expected: 'data')", header.data_id);
        // hw.PrintLine("fmt chunk found: %s", found_fmt ? "yes" : "no");
        // hw.PrintLine("data chunk found: %s", found_data ? "yes" : "no");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

        return found_fmt && found_data;
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
        hw.PrintLine("Data Offset: %lu", (unsigned long)header.data_offset);
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
            WavFile wav;
            if(wav.ParseHeader((const uint8_t*)inbuff))
            {
                wav.PrintHeader();
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
            fail = 1;
        }
    }
    else
    {
        hw.PrintLine("Can't Open file");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        fail = 2;
    }

}
