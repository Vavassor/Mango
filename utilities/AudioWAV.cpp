#include "AudioWAV.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace wave_audio {

#define WAVE_FORMAT_LPCM       0x0001
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

struct GUID
{
	uint32_t data_1;
	uint16_t data_2;
	uint16_t data_3;
	uint8_t  data_4[8];
};

#define DEFINE_WAVE_FORMAT_GUID(name, type) \
	const GUID name = \
	{ static_cast<uint32_t>(type), 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } }

DEFINE_WAVE_FORMAT_GUID(EXTENSION_GUID_PCM_C, WAVE_FORMAT_LPCM);
#define EXTENSION_GUID_PCM EXTENSION_GUID_PCM_C

DEFINE_WAVE_FORMAT_GUID(EXTENSION_GUID_IEEE_FLOAT_C, WAVE_FORMAT_IEEE_FLOAT);
#define EXTENSION_GUID_IEEE_FLOAT EXTENSION_GUID_IEEE_FLOAT_C

#define GUID_EQUALS(a, b) (!memcmp(&(a), &(b), sizeof(GUID)))

#define ALLOCATE(byte_count) \
	malloc(byte_count)
	
#define DEALLOCATE(pointer) \
	free(pointer)

namespace
{
	const char* failure_reason = nullptr;
}

static inline void concatenate(const char* input_a, const char* input_b, char* output)
{
	while((*output = *input_a++)) ++output;
	while((*output++ = *input_b++));
}

static inline int compare_strings(const char* a, const char* b, size_t n)
{
	while(n--)
	{
		if(*a++ != *b++)
			return *(unsigned char*)(a - 1) - *(unsigned char*)(b - 1);
	}
	return 0;
}

static inline bool tag_equals(const char* a, const char* b)
{
	return compare_strings(a, b, 4) == 0;
}

static inline uint32_t extract_integer(FILE* file)
{
	uint8_t b[4];
	fread(b, sizeof(uint8_t), 4, file);
	return (b[3] << 24) + (b[2] << 16) + (b[1] << 8) + (b[0]);
}

static inline uint16_t extract_word(FILE* file)
{
	uint8_t b[2];
	fread(b, sizeof(uint8_t), 2, file);
	return (b[1] << 8) + b[0];
}

static inline uint32_t pad_chunk_size(uint32_t x)
{
	// Chunks are aligned to even-numbered offsets, so the chunk actually
	// contains the specified number of bytes plus 0 or 1 padding byte.
	// So, round up the byte count to the nearest even number so the correct
	// number of bytes gets processed

	return (x + 1) & ~1;
}

#define FAILURE_TO_LOAD(message) \
{                                \
	failure_reason = message;    \
	result = false;              \
	goto cleanup;                \
}

bool load_whole_file(const char* filename, WaveData& wave)
{
	bool result = true;
	failure_reason = nullptr;

	char path[128];
	concatenate("resources/sounds/", filename, path);

	FILE* file = fopen(path, "rb");
	if(file == nullptr)
	{
		// exit early without cleanup because there's no file to close
		failure_reason = "couldn't open file";
		result = false;
		return result;
	}

	// loop through and process RIFF chunks
	for(;;)
	{
		char RIFF_tag[4];
		size_t RIFF_tag_bytes_read = fread(RIFF_tag, sizeof(char), 4, file);

		// exit loop if end of file is reached
		if(RIFF_tag_bytes_read != 4) break;

		uint32_t RIFF_chunk_size = extract_integer(file);

		if(tag_equals("RIFF", RIFF_tag))
		{
			// check the descriptor of the RIFF chunk to see if it contains WAVE subchunks

			char RIFF_descriptor[4];
			fread(RIFF_descriptor, sizeof(char), 4, file);
			if(!tag_equals("WAVE", RIFF_descriptor))
				FAILURE_TO_LOAD("RIFF chunk was not in WAVE format");

			// If it contains WAVE subchunks, read them in.

			// Loop through the subchunks until all bytes in the RIFF chunk have been processed.
			// Since the subchunk size is variable, increment the position when the size of
			// the subchunk is determined instead of at the end of each loop iteration

			for(uint32_t RIFF_chunk_position = sizeof(RIFF_descriptor); RIFF_chunk_position < RIFF_chunk_size; )
			{
				char tag[4];
				size_t tag_bytes_read = fread(tag, sizeof(char), 4, file);

				// if end of file is reached unexpectedly
				if(tag_bytes_read != 4) break;

				uint32_t chunk_size = pad_chunk_size(extract_integer(file));

				// increment position in RIFF chunk
				RIFF_chunk_position += sizeof(tag) + 4 + chunk_size;

				// process subchunk depending on type

				if(tag_equals("fmt ", tag))
				{
					//-----WAV Format Chunk-----

					uint16_t format_type = extract_word(file);

					wave.num_channels = extract_word(file);
					wave.sample_rate = extract_integer(file);
					wave.average_bytes_per_second = extract_integer(file);
					wave.block_alignment = extract_word(file);
					wave.bits_per_sample = extract_word(file);

					switch(format_type)
					{
						case WAVE_FORMAT_LPCM:
						{
							wave.format = WaveFormat::LPCM_Integer;

							// format extension is optional, but if it IS there it needs to
							// be skipped in order to process the next chunk
							if(chunk_size >= 18)
							{
								uint16_t extension_size = extract_word(file);
								if(extension_size > 0)
									fseek(file, extension_size, SEEK_CUR);
							}
						} break;

						case WAVE_FORMAT_IEEE_FLOAT:
						{
							wave.format = WaveFormat::LPCM_IEEE_Float;

							// pull out required extension size and skip if there is an extension

							uint16_t extension_size = extract_word(file);
							if(extension_size > 0)
								fseek(file, extension_size, SEEK_CUR);
						} break;

						case WAVE_FORMAT_EXTENSIBLE:
						{
							uint16_t extension_size = extract_word(file);

							uint16_t samples = extract_word(file);
							uint32_t channel_mask = extract_integer(file);

							GUID guid = {};
							guid.data_1 = extract_integer(file);
							guid.data_2 = extract_word(file);
							guid.data_3 = extract_word(file);
							fread(guid.data_4, sizeof(uint8_t), 8, file);

							if(GUID_EQUALS(EXTENSION_GUID_PCM, guid))
							{
								wave.format = WaveFormat::LPCM_Integer;
							}
							else if(GUID_EQUALS(EXTENSION_GUID_IEEE_FLOAT, guid))
							{
								wave.format = WaveFormat::LPCM_IEEE_Float;
							}
							else
							{
								FAILURE_TO_LOAD("wave format extension subtype not recognised");
							}
						} break;

						default:
						{
							FAILURE_TO_LOAD("wave format not recognised");
						} break;
					}

					if(ferror(file))
						FAILURE_TO_LOAD("WAVE Format subchunk was corrupt");
				}
				else if(tag_equals("fact", tag))
				{
					//-----WAV Fact Chunk-----

					wave.sample_count = extract_integer(file);

					if(ferror(file))
						FAILURE_TO_LOAD("corruption encountered in WAVE fact chunk");
				}
				else if(tag_equals("data", tag))
				{
					//-----WAV Sample Data Chunk-----

					if(wave.sample_count == 0)
						wave.sample_count = chunk_size / (wave.num_channels * wave.bits_per_sample / 8);

					void* sample_data = ALLOCATE(chunk_size);
					if(sample_data == nullptr)
						FAILURE_TO_LOAD("could not allocate buffer large enough for sample data");
					
					wave.data = sample_data;

					size_t data_bytes_read = fread(wave.data, sizeof(uint8_t), chunk_size, file);
					if(data_bytes_read != chunk_size)
						FAILURE_TO_LOAD("sample section is missing data. Fewer bytes read than expected");

					if(ferror(file))
						FAILURE_TO_LOAD("corruption encountered in WAVE sample data chunk");
				}
				else
				{
					// unused wave subchunks are skipped
					fseek(file, chunk_size, SEEK_CUR);
				}
			}
		}
		else
		{
			// since the RIFF chunk tag isn't used or recognized, skip over the chunk

			uint32_t padded_byte_count = pad_chunk_size(RIFF_chunk_size);
			fseek(file, padded_byte_count, SEEK_CUR);
		}
	}

cleanup:
	fclose(file);

	return result;
}

const char* load_failure_reason()
{
	return failure_reason;
}

void unload_wave_data(WaveData& wave)
{
	DEALLOCATE(wave.data);
	memset(&wave, 0, sizeof wave);
}

} // namespace wave_audio
