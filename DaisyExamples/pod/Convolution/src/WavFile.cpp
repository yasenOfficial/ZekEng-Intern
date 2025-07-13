#include "WavFile.h"
#include "daisy_seed.h"
#include <string.h>
#include "fatfs.h"

// Externally defined in your main file
extern daisy::DaisySeed hw;
extern FIL in_file;
extern FIL out_file;


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
    FRESULT fr;
    uint8_t header[256];
    UINT bytesread = 0, byteswritten = 0;

    // Open input file
    if(f_open(&in_file, infile_name, FA_READ) != FR_OK)
    {
        hw.PrintLine("Can't open input file");
        return false;
    }

    // Read header
    fr = f_read(&in_file, header, sizeof(header), &bytesread);
    if(fr != FR_OK || bytesread < 44) // WAV header is at least 44 bytes
    {
        hw.PrintLine("Can't read WAV header, f_read returned: %d", fr);
        f_close(&in_file);
        return false;
    }

    // Parse header
    WavFile wav;
    if(!wav.ParseHeader(header, bytesread))
    {
        hw.PrintLine("Not a valid WAV header!");
        f_close(&in_file);
        return false;
    }
    wav.PrintHeader();
    const WavFile::Header& h = wav.GetHeader();

    // Sanity check
    FILINFO fno;
    if(f_stat(infile_name, &fno) != FR_OK ||
       (unsigned long)h.data_offset + (unsigned long)h.data_size > (unsigned long)fno.fsize)
    {
        hw.PrintLine("ERROR: data_offset + data_size > filesize! WAV may be corrupt.");
        f_close(&in_file);
        return false;
    }

    // Open output file
    if(f_open(&out_file, outfilename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        hw.PrintLine("Can't open output file");
        f_close(&in_file);
        return false;
    }

    // Write header (up to data_offset)
    if(f_lseek(&in_file, 0) != FR_OK)
    {
        hw.PrintLine("Failed to seek input file");
        f_close(&in_file);
        f_close(&out_file);
        return false;
    }
    UINT headerRead = 0, headerWritten = 0;
    if(f_read(&in_file, header, h.data_offset, &headerRead) != FR_OK || headerRead != h.data_offset)
    {
        hw.PrintLine("Failed to read header from input file");
        f_close(&in_file);
        f_close(&out_file);
        return false;
    }
    if(f_write(&out_file, header, h.data_offset, &headerWritten) != FR_OK || headerWritten != h.data_offset)
    {
        hw.PrintLine("Failed to write header to output file");
        f_close(&in_file);
        f_close(&out_file);
        return false;
    }

    // Write audio data chunk by chunk
    if(f_lseek(&in_file, h.data_offset) != FR_OK)
    {
        hw.PrintLine("Failed to seek to data_offset in input file");
        f_close(&in_file);
        f_close(&out_file);
        return false;
    }

    size_t data_left = h.data_size;
    uint8_t* chunk = new uint8_t[kChunkSize];
    while(data_left > 0)
    {
        size_t to_read = (data_left > kChunkSize) ? kChunkSize : data_left;
        bytesread = byteswritten = 0;
        fr = f_read(&in_file, chunk, to_read, &bytesread);
        if(fr != FR_OK || bytesread == 0)
        {
            hw.PrintLine("Data read failed: res=%d bytes=%lu/%lu", fr, (unsigned long)bytesread, (unsigned long)to_read);
            break;
        }
        fr = f_write(&out_file, chunk, bytesread, &byteswritten);
        if(fr != FR_OK || byteswritten != bytesread)
        {
            hw.PrintLine("Data write failed: res=%d bytes=%lu/%lu", fr, (unsigned long)byteswritten, (unsigned long)bytesread);
            break;
        }
        data_left -= byteswritten;
        hw.PrintLine("Chunk: wrote %lu bytes, %lu left", (unsigned long)byteswritten, (unsigned long)data_left);
    }
    delete[] chunk;

    f_close(&in_file);
    f_close(&out_file);

    if(data_left == 0)
    {
        hw.PrintLine("Wrote WAV file chunked: %s (header: %lu, data: %lu)", outfilename, (unsigned long)h.data_offset, (unsigned long)h.data_size);
        return true;
    }
    else
    {
        hw.PrintLine("Failed to write entire WAV file (left: %lu)", (unsigned long)data_left);
        return false;
    }
}
