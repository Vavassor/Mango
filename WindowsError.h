#ifndef WINDOWS_ERROR_H
#define WINDOWS_ERROR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

void windows_error_message(DWORD error, const char* description);

#endif
