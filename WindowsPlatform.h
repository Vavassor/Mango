#ifndef WINDOWS_PLATFORM_H
#define WINDOWS_PLATFORM_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

bool create_window(HINSTANCE instance);
void destroy_window();
void show_window(int show_mode);
int message_loop();
void show_error_message();

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);

#endif
