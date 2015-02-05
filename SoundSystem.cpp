#include "SoundSystem.h"
#include "WindowsError.h"

#include "utilities/AudioWAV.h"

#include <mmdeviceapi.h>
#include <Ks.h>
#include <KsMedia.h>
#include <Audioclient.h>

#include <cmath>

#define M_TAU 6.28318530717958647692

namespace SoundSystem {

#define REFTIMES_PER_SECOND      10000000
#define REFTIMES_PER_MILLISECOND (REFTIMES_PER_SECOND / 1000)

namespace
{
	HANDLE thread = NULL;
	DWORD thread_ID;

	struct ThreadRequest
	{
		HRESULT result;
		HANDLE finished_event;
	};

	enum ThreadMessage
	{
		First = WM_USER,
		Quit,
		Begin_Playback,
		End_Playback,
		Last,
	};

	IAudioClient* audio_client = nullptr;
	IAudioRenderClient* render_client = nullptr;
	IAudioClock* device_clock = nullptr;
	WAVEFORMATEX* mix_format = nullptr;

	HANDLE buffer_event = NULL;

	enum class SampleType
	{
		Integer,
		IEEE_Float,
	} sample_type;

	UINT32 max_buffer_frames;
	UINT64 clock_frequency;
	UINT32 min_render_frames;
	BYTE* render_buffer = nullptr;

	bool paused = true;
	int pitch = 69;

	WaveData bloop_wave = {};
}

void Thread_Quit();
void On_Begin_Playback();
void On_End_Playback();
void Update();

#define SAFE_RELEASE(thing) \
	if((thing) != nullptr) \
	{ (thing)->Release(); (thing) = nullptr; }

#define EXIT_ON_ERROR(result, message) \
	if(FAILED(result)) \
	{ windows_error_message((result), (message)); goto cleanup; }

static inline void send_response_message(ThreadRequest* request, HRESULT result)
{
	request->result = result;
	SetEvent(request->finished_event);
}

static HRESULT wait_for_response(ThreadRequest* request)
{
	if(WaitForSingleObject(request->finished_event, INFINITE) == WAIT_OBJECT_0)
		return request->result;
	windows_error_message(request->result, "Message response error"); // TODO write a real error message
	return E_FAIL;
}

DWORD WINAPI Thread_Start(
	_In_ LPVOID parameter)
{
	ThreadRequest* request = static_cast<ThreadRequest*>(parameter);

	IMMDeviceEnumerator* device_enumerator = nullptr;
	IMMDevice* device = nullptr;

	HRESULT result = S_OK;

	// initialise COM library
	result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	EXIT_ON_ERROR(result, "failed to initialise COM library for mmdevapi");

	// fetch device enumerator
	result = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		NULL,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		reinterpret_cast<LPVOID*>(&device_enumerator));
	EXIT_ON_ERROR(result, "failed to create device enumerator");

	// get default audio output device
	result = device_enumerator->GetDefaultAudioEndpoint(
		EDataFlow::eRender,
		ERole::eConsole,
		&device);
	EXIT_ON_ERROR(result, "failed to retrieve default render endppoint");

	// create WASAPI audio client from the default device
	result = device->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL,
		nullptr,
		reinterpret_cast<void**>(&audio_client));
	EXIT_ON_ERROR(result, "failed to create audio client");

	// setup desired mix format
	{
		WORD num_channels = 2;
		WORD bit_rate = sizeof(float) * 8;
		DWORD sample_rate = 48000;

		WORD bytes_per_frame = num_channels * (bit_rate / 8);

		WAVEFORMATEXTENSIBLE desired_format = {};
		desired_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		desired_format.Format.nChannels = num_channels;
		desired_format.Format.nSamplesPerSec = sample_rate;
		desired_format.Format.nAvgBytesPerSec = sample_rate * bytes_per_frame;
		desired_format.Format.nBlockAlign = bytes_per_frame;
		desired_format.Format.wBitsPerSample = bit_rate;
		desired_format.Format.cbSize = sizeof(desired_format) - sizeof(WAVEFORMATEX);

		desired_format.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
		desired_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
		desired_format.Samples.wValidBitsPerSample = bit_rate;

		// Request the mix format
		{
			WAVEFORMATEX* closest_format = nullptr;
			result = audio_client->IsFormatSupported(
				AUDCLNT_SHAREMODE_SHARED,
				reinterpret_cast<WAVEFORMATEX*>(&desired_format),
				&closest_format);
			EXIT_ON_ERROR(result, "can't get a compatible mix format");

			// If the desired format is not supported, it will allocate and return the closest match
			// but if it IS supported, we have to allocate it ourselves and copy over the data
			// (it has to be allocated on the heap some way or another because we are keeping a
			// global copy called mix_format)
			if(result == S_OK && closest_format == nullptr)
			{
				closest_format = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
				CopyMemory(closest_format, &desired_format, sizeof(WAVEFORMATEXTENSIBLE));
			}

			// check to see if the closest match has a sample type we can handle, otherwise there's an error
			if(closest_format->wFormatTag == WAVE_FORMAT_PCM
				|| closest_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE
				&& reinterpret_cast<WAVEFORMATEXTENSIBLE*>(closest_format)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
			{
				sample_type = SampleType::Integer;
			}
			else if(closest_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
				|| closest_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE
				&& reinterpret_cast<WAVEFORMATEXTENSIBLE*>(closest_format)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			{
				sample_type = SampleType::IEEE_Float;
			}
			else
			{
				EXIT_ON_ERROR(result, "device mix format did not support any compatible sample types");
			}
			mix_format = closest_format;
		}
	}

	// initialise settings for the audio client
	result = audio_client->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		REFTIMES_PER_SECOND,
		0, // periodicity must be zero in shared mode
		reinterpret_cast<WAVEFORMATEX*>(mix_format),
		nullptr);
	EXIT_ON_ERROR(result, "failed to initialise audio client");

	// get render client from audio client
	result = audio_client->GetService(
		__uuidof(IAudioRenderClient),
		reinterpret_cast<void**>(&render_client));
	EXIT_ON_ERROR(result, "failed to get render client");

	// Create an event handle and register it for buffer-event notifications.
	buffer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	result = audio_client->SetEventHandle(buffer_event);
	EXIT_ON_ERROR(result, "failed to set buffer event handle");

	// get frame count from audio client
	result = audio_client->GetBufferSize(&max_buffer_frames);
	EXIT_ON_ERROR(result, "couldn't obtain buffer frames count");

	// get audio clock to monitor the stream and keep it synchronised
	result = audio_client->GetService(
		__uuidof(IAudioClock),
		reinterpret_cast<void**>(&device_clock));
	EXIT_ON_ERROR(result, "couldn't obtain audio clock for monitoring audio synchronisation");

	result = device_clock->GetFrequency(&clock_frequency);
	EXIT_ON_ERROR(result, "failed to get device clock frequency");

	// ask for device latency to use for synchronisation calculations
	{
		REFERENCE_TIME latency = 0;
		result = audio_client->GetStreamLatency(&latency);
		EXIT_ON_ERROR(result, "couldn't determine client stream latency");

		double seconds_of_latency = static_cast<double>(latency) / REFTIMES_PER_SECOND;
		min_render_frames = static_cast<double>(mix_format->nSamplesPerSec) * seconds_of_latency;
	}

	// Create a render buffer that's the same size as the client buffer.
	// It'll be used to store intermediate data that is to be written to
	// the render client every update
	{
		size_t bytes_per_sample = mix_format->wBitsPerSample / 8;
		size_t frame_size_in_bytes = mix_format->nChannels * bytes_per_sample;
		size_t max_buffer_size_in_bytes = frame_size_in_bytes * max_buffer_frames;
		render_buffer = new BYTE[max_buffer_size_in_bytes];
	}

	bool bloop_loaded = load_WAV_file("Bloop.wav", bloop_wave);
	if(!bloop_loaded)
	{
		char* failure_reason = WAV_load_failure_reason();
		result = 0xA0000001; // arbitrary code signified as a "customer" error code HRESULT
		EXIT_ON_ERROR(result, failure_reason);
	}

cleanup:
	SAFE_RELEASE(device);
	SAFE_RELEASE(device_enumerator);

	CoUninitialize();

	// thread initialisation failed
	if(FAILED(result))
	{
		send_response_message(request, result);
		return 0;
	}

	// before submitting the initialisation complete response, initialise the
	// message queue for this thread so it can start getting messages
	MSG message_info = {};
	PeekMessage(&message_info, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	// thread initialisation completed!
	send_response_message(request, S_OK);

	//------ MESSAGE LOOP ----------

	for(;;)
	{
		while(PeekMessage(
			&message_info,
			reinterpret_cast<HWND>(-1), // retrieves only messages whose window handle is NULL
			ThreadMessage::First,
			ThreadMessage::Last,
			PM_REMOVE))
		{
			switch(message_info.message)
			{
				case WM_QUIT:
				case ThreadMessage::Quit:
					goto quit;

				case ThreadMessage::Begin_Playback:
					On_Begin_Playback();
					break;

				case ThreadMessage::End_Playback:
					On_End_Playback();
					break;
			}
		}

		if(!paused)
		{
			DWORD wait_result = WaitForSingleObject(buffer_event, 1000);
			if(wait_result == WAIT_OBJECT_0)
				Update();
		}
	}

quit:
	// thread termination
	Thread_Quit();

	return 0;
}

void Thread_Quit()
{
	unload_wave_data(bloop_wave);

	delete[] render_buffer;

	if(!paused && audio_client)
	{
		HRESULT result = audio_client->Stop();
		if(FAILED(result))
		{
			windows_error_message(result, "client playback did not stop on shutdown");
		}
	}

	if(buffer_event) CloseHandle(buffer_event);
	CoTaskMemFree(mix_format);
	SAFE_RELEASE(audio_client);
	SAFE_RELEASE(render_client);
	SAFE_RELEASE(device_clock);
}

bool Initialise()
{
	if(thread) return false;

	HRESULT result = S_OK;

	ThreadRequest request;
	request.finished_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if(request.finished_event == NULL)
	{
		windows_error_message(0, "Failed to create event"); // TODO write a real error message
		return false;
	}

	thread = CreateThread(
		nullptr,
		0, // zero indicates to use default initial stack size
		Thread_Start,
		&request,
		0, // use default creation flags
		&thread_ID);
	if(thread) result = wait_for_response(&request);
	CloseHandle(request.finished_event);
	
	return SUCCEEDED(result);
}

void Terminate()
{
	if(thread)
	{
		PostThreadMessage(thread_ID, ThreadMessage::Quit, 0, 0);
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
		thread = NULL;
	}
}

void Play()
{
	PostThreadMessage(thread_ID, ThreadMessage::Begin_Playback, 0, 0);
	paused = false;
}

void Stop()
{
	PostThreadMessage(thread_ID, ThreadMessage::End_Playback, 0, 0);
	paused = true;
}

void On_Begin_Playback()
{
	HRESULT result = audio_client->Start();
}

void On_End_Playback()
{
	HRESULT result = audio_client->Stop();
}

void Generate_Integer_Sine_Samples(
	double frequency,
	double amplitude,
	double& theta,
	UINT32 num_frames,
	WORD num_channels)
{
	double time_step = 1.0 / static_cast<double>(mix_format->nSamplesPerSec);
	size_t bytes_per_sample = sizeof(INT16);

	for(int i = 0; i < num_frames; ++i)
	{
		double angle = M_TAU * frequency * theta;
		double factor = sin(angle) * amplitude;

		INT16 pcm_value = static_cast<INT16>(32767.0 * factor);
		BYTE* sample = reinterpret_cast<BYTE*>(&pcm_value);
		for(int j = 0; j < num_channels; ++j)
		{
			for(int k = 0; k < bytes_per_sample; ++k)
			{
				render_buffer[i * num_channels * bytes_per_sample + j * bytes_per_sample + k] = sample[k];
			}
		}

		theta += time_step;
	}
}

void Generate_Float_Sine_Samples(
	double frequency,
	double amplitude,
	double& theta,
	UINT32 num_frames,
	WORD num_channels)
{
	double time_step = 1.0 / static_cast<double>(mix_format->nSamplesPerSec);
	size_t bytes_per_sample = sizeof(FLOAT);

	for(int i = 0; i < num_frames; ++i)
	{
		double angle = M_TAU * frequency * theta;
		double factor = sin(angle) * amplitude;
		
		FLOAT pcm_value = factor;
		BYTE* sample = reinterpret_cast<BYTE*>(&pcm_value);
		for(int j = 0; j < num_channels; ++j)
		{
			for(int k = 0; k < bytes_per_sample; ++k)
			{
				render_buffer[i * num_channels * bytes_per_sample + j * bytes_per_sample + k] = sample[k];
			}
		}

		theta += time_step;
	}
}

static inline UINT32 least(UINT32 a, UINT32 b)
{
	UINT32 m = a;
	if(b < m) m = b;
	return m;
}

void Update()
{
	/*
	UINT64 device_position;
	device_clock->GetPosition(&device_position, nullptr);
	*/

	UINT32 num_frames_padding;
	audio_client->GetCurrentPadding(&num_frames_padding);
	UINT32 frames_available = max_buffer_frames - num_frames_padding;

	UINT32 num_render_frames = least(min_render_frames, frames_available);
	
	DWORD flags = 0;
	WORD num_channels = mix_format->nChannels;
	WORD bytes_per_sample = mix_format->wBitsPerSample / 8;

	double frequency = 440.0 * pow(2.0, static_cast<double>(pitch - 69) / 12.0);
	double amplitude = 0.3;

	static double theta = 0.0;

	switch(sample_type)
	{
		case SampleType::IEEE_Float:
		{
			Generate_Float_Sine_Samples(frequency, amplitude, theta, num_render_frames, num_channels);
			break;
		}
		case SampleType::Integer:
		{
			Generate_Integer_Sine_Samples(frequency, amplitude, theta, num_render_frames, num_channels);
			break;
		}
	}
	
	SIZE_T transfer_size = num_render_frames * num_channels * bytes_per_sample;
	BYTE* data = nullptr;
	render_client->GetBuffer(num_render_frames, &data);
	CopyMemory(data, render_buffer, transfer_size);
	render_client->ReleaseBuffer(num_render_frames, flags);
}

} // namespace SoundSystem