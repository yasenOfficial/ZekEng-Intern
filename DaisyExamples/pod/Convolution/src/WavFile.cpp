#include "WavFile.h"
#include "daisy_seed.h"
#include <string.h>
#include "fatfs.h"

// Externally defined in your main file
extern daisy::DaisySeed hw;

WavFile::WavFile() {}

bool WavFile::ParseHeader(const uint8_t* buf, size_t bufsize)
{
    memset(&header, 0, sizeof(header));
    header.data_offset = 0;

    memcpy(header.riff_id, buf, 4);
    header.riff_id[4] = '\0';
    header.file_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    memcpy(header.wave_id, buf + 8, 4);
    header.wave_id[4] = '\0';

    if(strcmp(header.riff_id, "RIFF") != 0
       || strcmp(header.wave_id, "WAVE") != 0)
        return false;

    size_t offset    = 12;
    bool   found_fmt = false, found_data = false;

    while(offset + 8 <= bufsize)
    {
        char chunk_id[5];
        memcpy(chunk_id, buf + offset, 4);
        chunk_id[4]         = '\0';
        uint32_t chunk_size = buf[offset + 4] | (buf[offset + 5] << 8)
                              | (buf[offset + 6] << 16)
                              | (buf[offset + 7] << 24);

        if(strcmp(chunk_id, "fmt ") == 0 && chunk_size >= 16)
        {
            found_fmt = true;
            memcpy(header.fmt_id, chunk_id, 5);
            header.fmt_size     = chunk_size;
            header.audio_format = buf[offset + 8] | (buf[offset + 9] << 8);
            header.num_channels = buf[offset + 10] | (buf[offset + 11] << 8);
            header.sample_rate  = buf[offset + 12] | (buf[offset + 13] << 8)
                                 | (buf[offset + 14] << 16)
                                 | (buf[offset + 15] << 24);
            header.byte_rate = buf[offset + 16] | (buf[offset + 17] << 8)
                               | (buf[offset + 18] << 16)
                               | (buf[offset + 19] << 24);
            header.block_align     = buf[offset + 20] | (buf[offset + 21] << 8);
            header.bits_per_sample = buf[offset + 22] | (buf[offset + 23] << 8);
        }
        else if(strcmp(chunk_id, "data") == 0)
        {
            found_data = true;
            memcpy(header.data_id, chunk_id, 5);
            header.data_size   = chunk_size;
            header.data_offset = offset + 8;
        }
        offset += 8 + ((chunk_size + 1) & ~1);
    }

    daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();

    return found_fmt && found_data;
}

const WavFile::Header& WavFile::GetHeader() const
{
    return header;
}

void WavFile::PrintHeader()
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

bool CopyWavFileChunked(const char* infile_name,
                        const char* outfilename,
                        size_t      kChunkSize)
{
    DIR     dir;
    FILINFO fno;
    FIL SDFile;
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
                    // wav.WriteAsNewFile("rebuilt.wav", full_file);
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

    return true;
}

static FIL g_SDFile;

bool ReadAndPrintWavHeader(const char* filename)
{
    FIL File;
    uint8_t header[256];

    // Open file
    if(f_open(&g_SDFile, filename, FA_READ) != FR_OK)
    {
        hw.PrintLine("Can't Open file");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        return false;
    }

    // Read header
    UINT bytesread_total = 0;
    FRESULT fr = f_read(&g_SDFile, header, sizeof(header), &bytesread_total);
    f_close(&g_SDFile);

    if(fr == FR_OK && bytesread_total == sizeof(header))
    {
        WavFile wav;
        if(wav.ParseHeader(header, sizeof(header)))
        {
            wav.PrintHeader();
            // You can access h here if needed
            const WavFile::Header& h = wav.GetHeader();
            return true;
        }
        else
        {
            hw.PrintLine("Not a valid WAV header!");
            daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
            return false;
        }
    }
    else
    {
        hw.PrintLine(
            "Can't read file, f_read returned: %d (read %lu/%lu bytes)",
            fr,
            (unsigned long)bytesread_total,
            (unsigned long)sizeof(header));
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        return false;
    }
}
