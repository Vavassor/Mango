#include "WindowsPlatform.h"
#include "WindowsError.h"
#include "resource.h"
#include "RenderSystem.h"
#include "SoundSystem.h"
#include "Game.h"
#include "Input.h"

#include "utilities/Logging.h"

#include "gl_core_3_3.h"
#include <GL/gl.h>
#include <GL/glext.h>

#include "wgl_extensions.h"
#include <GL/wglext.h>

namespace
{
	HWND window = NULL;
	const char* window_title = "MANGO";
	int target_width = 160;
	int target_height = 144;
	int scale = 2;
	int width = scale * target_width;
	int height = scale * target_height;

	bool vertical_synchronization = true;

	HDC device_context = NULL;
	HGLRC rendering_context = NULL;

	byte_t input_state = 0;

	double performance_counter_resolution;
	double target_seconds_per_frame = 1.0 / 60.0;
	LARGE_INTEGER last_counter;

	bool paused = false;

	bool render_system_initialised = false;
}

static LARGE_INTEGER get_time_counter()
{
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result;
}

static double get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	return static_cast<double>(end.QuadPart - start.QuadPart)
		/ performance_counter_resolution;
}

static void system_error_message(const char* text)
{
	DWORD error = GetLastError();
	windows_error_message(error, text);
}

bool create_window(HINSTANCE instance)
{
	// setup window class
	WNDCLASSEXA window_class = {};
	window_class.cbSize = sizeof window_class;
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = instance;
	window_class.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1));
	window_class.hIconSm = static_cast<HICON>(LoadImage(instance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0));
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = "MangoWindowClass";

	if(RegisterClassExA(&window_class) == 0)
	{
		system_error_message("couldn't register window class");
		return false;
	}

	// create the actual window
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	window = CreateWindowExA(WS_EX_APPWINDOW, window_class.lpszClassName, window_title, style,
		0, 0, width, height, NULL, NULL, instance, NULL);
	if(window == NULL)
	{
		system_error_message("window creation failed");
		return false;
	}

	// initialize device context
	if((device_context = GetDC(window)) == NULL)
	{
		LOG_ISSUE("couldn't get device context for this window");
		return false;
	}

	// set pixel format for device context
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof pfd;
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DEPTH_DONTCARE;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int format_index;
	if((format_index = ChoosePixelFormat(device_context, &pfd)) == 0)
	{
		system_error_message("matching pixel format for the device context could not be found (ChoosePixelFormat failed)");
		return false;
	}

	if(SetPixelFormat(device_context, format_index, &pfd) == FALSE)
	{
		system_error_message("couldn't set pixel format for the device context");
		return false;
	}

	// create rendering context
	if((rendering_context = wglCreateContext(device_context)) == NULL)
	{
		system_error_message("couldn't create rendering context");
		return false;
	}

	// set it to be this thread's rendering context
	if(wglMakeCurrent(device_context, rendering_context) == FALSE)
	{
		system_error_message("couldn't set this thread's rendering context (wglMakeCurrent failed)");
		return false;
	}

	// get procedures from OpenGL .dll
	{
		int loaded = ogl_LoadFunctions();
		if(loaded != ogl_LOAD_SUCCEEDED)
		{
			if(loaded == ogl_LOAD_FAILED)
			{
				LOG_ISSUE("couldn't load OpenGL procedures at all");
			}
			else
			{
				int num_failed = loaded - ogl_LOAD_SUCCEEDED;
				LOG_ISSUE("couldn't load OpenGL procedures: %i procedures failed to load", num_failed);
			}
			return false;
		}
	}

	// get wgl procedures
	{
		int loaded = wgl_LoadFunctions(device_context);
		if(loaded != wgl_LOAD_SUCCEEDED)
		{
			if(loaded == wgl_LOAD_FAILED)
			{
				LOG_ISSUE("couldn't fetch WGL extensions: couldn't load procedure wglGetExtensionsStringARB to even check for available extensions");
			}
			else
			{
				int num_failed = loaded - wgl_LOAD_SUCCEEDED;
				LOG_ISSUE("couldn't fetch WGL extensions: %i procedures failed to load", num_failed);
			}
			return false;
		}
	}

	// set vertical synchronization
	if(wgl_ext_EXT_swap_control != wgl_LOAD_FAILED)
	{
		if(wglSwapIntervalEXT((vertical_synchronization) ? 1 : 0) == FALSE)
		{
			system_error_message("swap interval was not set; continuing without vertical synchronization");
			vertical_synchronization = false;
		}
	}

	// set up non-platform-specific opengl things
	render_system_initialised = RenderSystem::Initialise(target_width, target_height, 2);
	if(!render_system_initialised)
		return false;

	// get AudioSystem going
	if(!SoundSystem::Initialise())
	{
		LOG_ISSUE("Audio System failed to load");
		return false;
	}
	SoundSystem::Play();

	// start up the game
	Game::Initialise();

	// start timer
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		performance_counter_resolution = static_cast<double>(frequency.QuadPart);

		last_counter = get_time_counter();
	}

	return true;
}

void destroy_window()
{
	Game::Terminate();

	SoundSystem::Stop();
	SoundSystem::Terminate();

	if(render_system_initialised)
		RenderSystem::Terminate();

	if(wglDeleteContext(rendering_context) == FALSE)
	{
		system_error_message("failed to delete rendering context on shutdown");
	}
}

void show_window(int show_mode)
{
	MONITORINFO monitor = {};
	monitor.cbSize = sizeof monitor;
	GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);

	int desktop_width = monitor.rcMonitor.right - monitor.rcMonitor.left;
	int desktop_height = monitor.rcMonitor.bottom - monitor.rcMonitor.top;

	RECT window_rect, client_rect;
	GetWindowRect(window, &window_rect);
	GetClientRect(window, &client_rect);

	int border_width = window_rect.right + (width - client_rect.right);
	int border_height = window_rect.bottom + (height - client_rect.bottom);

	border_width -= window_rect.left;
	border_height -= window_rect.top;

	int x = desktop_width / 2 - border_width / 2;
	int y = desktop_height / 2 - border_height / 2;

	MoveWindow(window, x, y, border_width, border_height, FALSE);
	ShowWindow(window, show_mode);
}

int message_loop()
{
	MSG msg = {};
	bool quit = false;
	while(!quit)
	{
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT) quit = true;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Log::Inc_Time();

		// determine time since last update
		LARGE_INTEGER now = get_time_counter();
		double delta_time = get_seconds_elapsed(last_counter, now);
		last_counter = now;

		Game::GameState game_state = Game::Update(input_state, delta_time);
		RenderSystem::Update(game_state);

		if(!vertical_synchronization)
		{
			// sleep away the rest of the frame
			LARGE_INTEGER now = get_time_counter();
			double frame_time_thusfar = get_seconds_elapsed(last_counter, now);
			if(frame_time_thusfar < target_seconds_per_frame)
			{
				DWORD time_to_sleep = (target_seconds_per_frame - frame_time_thusfar) * 1000.0;
				Sleep(time_to_sleep);
			}
		}

		SwapBuffers(device_context);
	}

	return msg.wParam;
}

#define MESSAGE_BOX_MAX_CHARS 1024

void show_error_message()
{
	const char* text = Log::Get_Text();

	WCHAR display_buffer[MESSAGE_BOX_MAX_CHARS];
	int char_count = MultiByteToWideChar(CP_UTF8, 0, text, -1, display_buffer, MESSAGE_BOX_MAX_CHARS);
	if (char_count > 0)
	{
		int response = MessageBoxW(window, display_buffer, L"Error", MB_OK | MB_ICONERROR);
	}
	else
	{
		// if the UTF-8 to wide-character string conversion fails, display the string as ANSI-encoded
		int response = MessageBoxA(window, text, "Error", MB_OK | MB_ICONERROR);
	}
}

static LRESULT on_close(HWND hwnd)
{
	if(ReleaseDC(window, device_context) == 0)
	{
		LOG_ISSUE("failed to release device context on shutdown");
	}

	if(DestroyWindow(hwnd) == FALSE)
	{
		system_error_message("couldn't destroy the window??");
	}

	return 0;
}

static LRESULT on_activate(WORD state)
{
	switch(state)
	{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
		{
			paused = false;
			break;
		}
		case WA_INACTIVE:
		{
			paused = true;
			break;
		}
	}

	return 0;
}

static inline byte_t get_key_mask(USHORT key)
{
	switch(key)
	{
		case VK_UP:    return INPUT_UP;
		case VK_DOWN:  return INPUT_DOWN;
		case VK_LEFT:  return INPUT_LEFT;
		case VK_RIGHT: return INPUT_RIGHT;
		case 'Z':      return INPUT_B;
		case 'X':      return INPUT_A;
		case 'W':      return INPUT_START;
		case 'Q':      return INPUT_SELECT;
	}
	return 0x00;
}

static LRESULT on_key_down(USHORT key)
{
	input_state |= get_key_mask(key);
	return 0;
}

static LRESULT on_key_up(USHORT key)
{
	input_state &= ~get_key_mask(key);
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
	switch(message)
	{
		case WM_CLOSE: return on_close(hwnd);
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_ACTIVATE: return on_activate(LOWORD(w_param));
		case WM_KEYDOWN: return on_key_down(w_param);
		case WM_KEYUP: return on_key_up(w_param);
	}
	return DefWindowProc(hwnd, message, w_param, l_param);
}