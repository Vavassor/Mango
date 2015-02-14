#include "WindowsPlatform.h"

#include "utilities/Logging.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cwchar>

#define W_(code) L ## code

#define EXCEPTION_CASE(code) \
	case code:               \
	return W_(#code);

#define EXCEPTION_MSC 0xE06D7363

const wchar_t* get_exception_text(DWORD code)
{
	switch(code)
	{
		EXCEPTION_CASE(EXCEPTION_ACCESS_VIOLATION);
		EXCEPTION_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
		EXCEPTION_CASE(EXCEPTION_BREAKPOINT);
		EXCEPTION_CASE(EXCEPTION_DATATYPE_MISALIGNMENT);

		EXCEPTION_CASE(EXCEPTION_FLT_DENORMAL_OPERAND);
		EXCEPTION_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO);
		EXCEPTION_CASE(EXCEPTION_FLT_INEXACT_RESULT);
		EXCEPTION_CASE(EXCEPTION_FLT_INVALID_OPERATION);
		EXCEPTION_CASE(EXCEPTION_FLT_OVERFLOW);
		EXCEPTION_CASE(EXCEPTION_FLT_STACK_CHECK);
		EXCEPTION_CASE(EXCEPTION_FLT_UNDERFLOW);

		EXCEPTION_CASE(EXCEPTION_GUARD_PAGE);
		EXCEPTION_CASE(EXCEPTION_ILLEGAL_INSTRUCTION);
		EXCEPTION_CASE(EXCEPTION_IN_PAGE_ERROR);
		EXCEPTION_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO);
		EXCEPTION_CASE(EXCEPTION_INT_OVERFLOW);
		EXCEPTION_CASE(EXCEPTION_INVALID_DISPOSITION);
		EXCEPTION_CASE(EXCEPTION_INVALID_HANDLE);
		EXCEPTION_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION);
		EXCEPTION_CASE(EXCEPTION_PRIV_INSTRUCTION);
		EXCEPTION_CASE(EXCEPTION_SINGLE_STEP);
		EXCEPTION_CASE(EXCEPTION_STACK_OVERFLOW);

		case EXCEPTION_MSC: return L"C++ exception (using throw)";
	}
	return L"Unknown exception";
}

LONG WINAPI unhandled_exception_filter(
	_In_ LPEXCEPTION_POINTERS info)
{
	if(!(GetErrorMode() & SEM_NOGPFAULTERRORBOX))
	{
		const wchar_t* exception_text = get_exception_text(info->ExceptionRecord->ExceptionCode);

		wchar_t message[255];
		swprintf(
			message,
			255,
			L"An exception occurred which wasn't handled.\nCode: %s (0x%08X)\nAddress: 0x%08X",
			exception_text,
			info->ExceptionRecord->ExceptionCode,
			info->ExceptionRecord->ExceptionAddress);
		MessageBoxW(NULL, message, L"Error", MB_ICONERROR | MB_OK);
	}
	
	return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(
	_In_ HINSTANCE instance,
	_In_opt_ HINSTANCE previous_instance,
	_In_ LPSTR command_line,
	_In_ int show_mode)
{
	UNREFERENCED_PARAMETER(previous_instance);
	UNREFERENCED_PARAMETER(command_line);

	SetUnhandledExceptionFilter(unhandled_exception_filter);

	// reset log file so initialization errors can be recorded
	Log::Clear_File();

	int main_return = 0;
	if(create_window(instance))
	{
		show_window(show_mode);
		main_return = message_loop();
	}
	else
	{
		show_error_message();
	}

	destroy_window();

	// flush remaining log messages
	Log::Output();

	return main_return;
}