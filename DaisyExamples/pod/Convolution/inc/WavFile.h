#pragma once
#include <stdint.h>
#include <stddef.h>
#include "fatfs.h"
#define IMPULSE_RESPONSE "out48.wav"

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

bool CopyWavFileChunked(const char* infile_name, const char* outfilename, size_t kChunkSize);
bool ReadAndPrintWavHeader(const char* filename);