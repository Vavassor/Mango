#ifndef AUDIO_WAV_H

#include <cstdint>

namespace wave_audio {

enum class WaveFormat
{
	Unknown,
	LPCM_Integer,
	LPCM_IEEE_Float,
};

struct WaveData
{
	WaveFormat format;

	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t average_bytes_per_second;
	uint16_t block_alignment;
	uint16_t bits_per_sample;

	void* data;
};

bool load_whole_file(const char* filename, WaveData& data);
const char* load_failure_reason();

void unload_wave_data(WaveData& data);

} // namespace wave_audio

#define AUDIO_WAV_H
#endif
