#include "WindowsError.h"

#include "utilities/Logging.h"

void windows_error_message(DWORD error, const char* description)
{
	// get error message formatted as a UTF-16 string to preserve any unicode in the message
	LPVOID message_buffer = NULL;
	DWORD char_count = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, // unused, meant for an unformatted string into which formatted message is to be inserted
		error,
		0, // unspecified language ID (LANGID)
		reinterpret_cast<LPWSTR>(&message_buffer),
		0, // buffer size unspecified because it is being allocated to size by the OS (ALLOCATE_BUFFER flag)
		NULL); // variable argument list used in the case of the ARGUMENT_ARRAY flag

	// if error message for given code was not found, log the code value and return
	if(char_count == 0)
	{
		LOG_ISSUE("%s: code 0x%x", description, error);
		return;
	}

	// since the log assumes UTF-8, take the returned message and convert the character set

	// determine the number of bytes needed to hold the UTF-8 string
	LPWSTR wide_string = reinterpret_cast<LPWSTR>(message_buffer);
	int byte_count = WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, NULL, 0, NULL, NULL);

	// allocate a buffer for the UTF-8 string and do the conversion
	LPVOID log_buffer = LocalAlloc(LPTR, byte_count);
	LPSTR utf8_string = reinterpret_cast<LPSTR>(log_buffer);
	WideCharToMultiByte(
		CP_UTF8,
		0x00,
		wide_string,
		-1, // -1 means the input buffer is null-terminated instead of explicit size
		utf8_string,
		byte_count,
		NULL, // non-representable character replacement - must both be NULL for CP_UTF8
		NULL);

	// finally, log the UTF-8 message and free the intermediate buffers
	LOG_ISSUE("%s: %s", description, utf8_string);

	LocalFree(log_buffer);
	LocalFree(message_buffer);
}
