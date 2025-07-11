#include "WavFile.h"
#include "daisy_seed.h"
#include <string.h>

// Externally defined in your main file
extern daisy::DaisySeed hw;

WavFile::WavFile() {}

bool WavFile::ParseHeader(const uint8_t* buf, size_t bufsize)
{
    memset(&header, 0, sizeof(header));
    header.data_offset = 0;

    memcpy(header.riff_id, buf, 4); header.riff_id[4] = '\0';
    header.file_size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
    memcpy(header.wave_id, buf + 8, 4); header.wave_id[4] = '\0';

    if(strcmp(header.riff_id, "RIFF") != 0 || strcmp(header.wave_id, "WAVE") != 0)
        return false;

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
        }
        else if(strcmp(chunk_id, "data") == 0)
        {
            found_data = true;
            memcpy(header.data_id, chunk_id, 5);
            header.data_size = chunk_size;
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

bool WavFile::WriteAsNewFile(const char* filename, const uint8_t* originalFileBuffer) const
{
    FIL outFile;
    UINT bytesWritten1 = 0;
    const Header& h = header;
    const size_t kChunkSize = 64; // Or 1024 if you have more RAM

    // Open destination file for writing (overwrite if exists)
    if(f_open(&outFile, filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        hw.PrintLine("Failed to open output file for writing");
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        return false;
    }

    // --- Write header (from start up to data_offset) ---
    FRESULT res1 = f_write(&outFile, originalFileBuffer, h.data_offset, &bytesWritten1);

    // --- Write audio data (from data_offset, data_size bytes) in chunks ---
    UINT totalWritten2 = 0;
    FRESULT res2 = FR_OK;

    if(res1 == FR_OK && bytesWritten1 == h.data_offset)
    {
        size_t data_left = h.data_size;
        size_t offset = 0;
        const uint8_t* pcm = originalFileBuffer + h.data_offset;
        UINT bytesWrittenChunk = 0;

        while(data_left > 0)
        {
            size_t to_write = (data_left > kChunkSize) ? kChunkSize : data_left;
            res2 = f_write(&outFile, pcm + offset, to_write, &bytesWrittenChunk);
            if(res2 != FR_OK || bytesWrittenChunk != to_write)
            {
                hw.PrintLine("Chunked write failed: res2=%d bytes=%lu/%lu", res2, (unsigned long)bytesWrittenChunk, (unsigned long)to_write);
                break;
            }
            offset     += bytesWrittenChunk;
            totalWritten2 += bytesWrittenChunk;
            data_left  -= bytesWrittenChunk;
        }
    }

    f_close(&outFile);

    if(res1 == FR_OK && res2 == FR_OK
       && bytesWritten1 == h.data_offset
       && totalWritten2 == h.data_size)
    {
        hw.PrintLine("Wrote WAV: %s (header: %lu, data: %lu)", filename, (unsigned long)h.data_offset, (unsigned long)h.data_size);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        return true;
    }
    else
    {
        hw.PrintLine("Failed to write WAV (header: %lu/%lu, data: %lu/%lu)",
                     (unsigned long)bytesWritten1, (unsigned long)h.data_offset,
                     (unsigned long)totalWritten2, (unsigned long)h.data_size);
        daisy::Logger<daisy::LOGGER_INTERNAL>::Flush();
        return false;
    }
}
