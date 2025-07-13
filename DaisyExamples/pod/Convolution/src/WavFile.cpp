#include "WavFile.h"
#include "daisy_seed.h"
#include <string.h>
#include "fatfs.h"

// Externally defined in your main file
extern daisy::DaisySeed hw;
extern FIL ir_file;
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


size_t LoadImpulseResponse(const char* ir_filename, int16_t* buffer, size_t maxlen, size_t* ir_offset, size_t* ir_len)
{
    uint8_t header[256];
    if(f_open(&ir_file, ir_filename, FA_READ) != FR_OK) return 0;

    UINT bytesread = 0;
    f_read(&ir_file, header, sizeof(header), &bytesread);

    WavFile irwav;
    if(!irwav.ParseHeader(header, bytesread)) { f_close(&ir_file); return 0; }
    const WavFile::Header& h = irwav.GetHeader();

    *ir_offset = h.data_offset;
    *ir_len = h.data_size / 2; // 2 bytes per 16-bit sample

    if(*ir_len > maxlen) *ir_len = maxlen;

    // Seek to data
    f_lseek(&ir_file, h.data_offset);
    UINT read;
    f_read(&ir_file, buffer, (*ir_len)*2, &read);
    f_close(&ir_file);

    return *ir_len;
}

bool ConvolveFiles(const char* in_filename, const char* ir_filename, const char* out_filename, size_t chunk_size)
{
    // 1. Load IR to RAM
    const size_t MAX_IR = 16384; // 16k samples = 32kB
    int16_t ir[MAX_IR];
    size_t ir_offset = 0, ir_len = 0;

    if(LoadImpulseResponse(ir_filename, ir, MAX_IR, &ir_offset, &ir_len) == 0)
    {
        hw.PrintLine("Failed to load IR");
        return false;
    }
    hw.PrintLine("Loaded IR: %lu samples", (unsigned long)ir_len);
   
    if(f_open(&in_file, in_filename, FA_READ) != FR_OK)
    {
        hw.PrintLine("Can't open input file!");
        return false;
    }
    if(f_open(&out_file, out_filename, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        f_close(&in_file);
        hw.PrintLine("Can't open output file!");
        return false;
    }

    // 3. Parse input header
    uint8_t header[256];
    UINT bytesread = 0;
    f_read(&in_file, header, sizeof(header), &bytesread);
    WavFile inwav;
    if(!inwav.ParseHeader(header, bytesread))
    {
        hw.PrintLine("Not a valid input WAV!");
        f_close(&in_file); f_close(&out_file); return false;
    }
    const WavFile::Header& inh = inwav.GetHeader();

    // Write output header (will patch sizes at the end)
    f_write(&out_file, header, inh.data_offset, &bytesread);

    // 4. Allocate buffers
    int16_t* input_chunk = new int16_t[chunk_size + ir_len];
    memset(input_chunk, 0, (chunk_size + ir_len)*2);
    int16_t* out_chunk = new int16_t[chunk_size];

    // Fill initial overlap with zeros for convolution
    size_t n_read = 0, total_written = 0;
    size_t chunk_samples = chunk_size;
    size_t prev = 0;

    // Seek to start of data
    f_lseek(&in_file, inh.data_offset);

    do {
        // Shift overlap from previous chunk
        memmove(input_chunk, input_chunk + chunk_samples, ir_len*2);
        memset(input_chunk + ir_len, 0, chunk_samples*2); // Zero clear

        UINT bytes;
        f_read(&in_file, input_chunk + ir_len, chunk_samples*2, &bytes);
        n_read = bytes / 2;

        // For each output sample in chunk
        for(size_t n = 0; n < n_read; ++n)
        {
            int32_t acc = 0;
            for(size_t k = 0; k < ir_len; ++k)
            {
                if(n + ir_len >= k) // Always true for n >= 0, ir_len >= 0
                    acc += input_chunk[n + ir_len - k] * ir[k];
            }
            // Clip to int16
            if(acc > 32767) acc = 32767;
            if(acc < -32768) acc = -32768;
            out_chunk[n] = acc;
        }

        // Write output
        f_write(&out_file, out_chunk, n_read*2, &bytes);
        total_written += bytes;

        // If less than a full chunk was read, we're done
        if(n_read < chunk_samples)
            break;

    } while(1);

    delete[] input_chunk;
    delete[] out_chunk;

    // Fix output header (write correct sizes)
    // (seek to file size & data size locations in header and update them)

    f_close(&in_file);
    f_close(&out_file);

    hw.PrintLine("Convolution done, wrote %lu bytes", (unsigned long)total_written);
    return true;
}

