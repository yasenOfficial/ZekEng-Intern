#pragma once
#include <stdint.h>
#include <stddef.h>
#include "fatfs.h"

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

    WavFile();
    bool ParseHeader(const uint8_t* buf, size_t bufsize = 512);
    const Header& GetHeader() const;
    void PrintHeader();


  private:
    Header header;
};
static FIL in_file;
static FIL ir_file;
static FIL out_file;

bool CopyWavFileChunked(const char* infile, const char* outfile, size_t kChunkSize);
bool ConvolveFiles(const char* infile, const char* irfile, const char* outfile, size_t chunk_size);
