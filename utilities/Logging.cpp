#include "Logging.h"

#include "String.h"
#include "FileHandling.h"
#include "Conversion.h"

#if defined(_MSC_VER) && defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <time.h>
#include <stdarg.h>

#define LOG_FILE_NAME "log_file.txt"

namespace Log
{
	String stream;
	long ticks;
}

void Log::Clear_File()
{
	clear_file(LOG_FILE_NAME);
}

void Log::Inc_Time()
{
	ticks++;
}

void Log::Output(bool printToConsole)
{
	if(stream.Size() == 0) return;

	#if defined(_DEBUG)
	if(printToConsole)
	{
		#if defined(_MSC_VER) && defined(_WIN32)
			OutputDebugStringA(stream.Data());
		#else
			printf("Log-%s", stream.Data());
		#endif
	}
	#endif

	save_text_file(stream.Data(), stream.Size(), LOG_FILE_NAME, FILE_MODE_APPEND);

	stream.Clear();
}

static const char* log_level_name(Log::Level level)
{
	switch(level)
	{
		case Log::Level::Error: return "Error";
		case Log::Level::Info:  return "Info";
		case Log::Level::Debug: return "Debug";
	}
	return "";
}

#define LOG_CASE(CASE_TYPE, ParamType, convert_function)\
	case CASE_TYPE:										\
	{													\
		ParamType param = va_arg(arguments, ParamType);	\
		convert_function(param, str);					\
		break;											\
	}

void Log::Add(Level level, const char* format, ...)
{
	// append log header to line
	stream.Reserve(stream.Size() + 100);

	time_t signature = time(nullptr);
	tm* t = localtime(&signature);

	char timeStr[20];
	int_to_string(t->tm_hour, timeStr);
	stream.Append(timeStr);
	stream.Append(":");

	int_to_string(t->tm_min, timeStr);
	stream.Append(timeStr);
	stream.Append(":");

	int_to_string(t->tm_sec, timeStr);
	stream.Append(timeStr);
	stream.Append("/");

	int_to_string(ticks, timeStr);
	stream.Append(timeStr);
	stream.Append(" ");

	stream.Append(log_level_name(level));
	stream.Append(": ");

	// write parameter data to log
	va_list arguments;
	va_start(arguments, format);

	enum ArgumentSize
	{
		MEDIAN,
		BYTE,
		SHORT,
		LONG,
		LONG_LONG,
	} sizeType;

	enum Mode
	{
		APPEND,
		INSERT,
	} mode;

	mode = Mode::APPEND;
	sizeType = ArgumentSize::MEDIAN;

	char* s = (char*) format;
	char* f = s;
	while(*s)
	{
		switch(mode)
		{
			case Mode::APPEND:
			{
				// go through string until '%' is found, which
				// signifies a sequence to be replaced
				if(*s == '%')
				{
					if(s != f)
					{
						stream.Append(f, s);
					}
					mode = Mode::INSERT;
					sizeType = ArgumentSize::MEDIAN;
				}
				break;
			}

			case Mode::INSERT:
			{
				// determine type of argument in parameter list and replace
				// the '%' sub-sequence with the parsed string as directed

				// size specifiers
				if(*s == '.')
				{

				}
				else if(*s == 'h')
				{
					sizeType = (sizeType == ArgumentSize::SHORT)? ArgumentSize::BYTE : ArgumentSize::SHORT;
					break;
				}
				else if(*s == 'l')
				{
					sizeType = (sizeType == ArgumentSize::LONG)? ArgumentSize::LONG : ArgumentSize::LONG_LONG;
					break;
				}

				// number types
				if(*s == 'i')
				{
					char str[20];
					switch(sizeType)
					{
						LOG_CASE(ArgumentSize::BYTE, char, int_to_string)
						LOG_CASE(ArgumentSize::SHORT, short, int_to_string)
						LOG_CASE(ArgumentSize::MEDIAN, int, int_to_string)
						LOG_CASE(ArgumentSize::LONG, long, int_to_string)
						LOG_CASE(ArgumentSize::LONG_LONG, long long, int_to_string)
					}
					stream.Append(str);
				}
				else if(*s == 'u')
				{
					char str[20];
					switch(sizeType)
					{
						LOG_CASE(ArgumentSize::BYTE, char, int_to_string)
						LOG_CASE(ArgumentSize::SHORT, short, int_to_string)
						LOG_CASE(ArgumentSize::MEDIAN, int, int_to_string)
						LOG_CASE(ArgumentSize::LONG, long, int_to_string)
						LOG_CASE(ArgumentSize::LONG_LONG, long long, int_to_string)
					}
					stream.Append(str);
				}
				else if(*s == 'x')
				{
					char str[20];
					unsigned long long param = va_arg(arguments, unsigned long long);
					int_to_string(param, str, 16);
					stream.Append(str);
				}
				else if(*s == 'f')
				{
					char str[32];
					switch(sizeType)
					{
						case ArgumentSize::BYTE:
						case ArgumentSize::SHORT:
						case ArgumentSize::MEDIAN:
						case ArgumentSize::LONG:
						case ArgumentSize::LONG_LONG:
						{
							double param = va_arg(arguments, double);
							float_to_string(param, str);
							break;
						}
					}
					stream.Append(str);
				}
				// text types
				else if(*s == 's')
				{
					const char* param = va_arg(arguments, const char*);
					stream.Append(param);
				}

				f = s + 1;
				mode = Mode::APPEND;

				break;
			}
		}

		++s;
	}

	va_end(arguments);

	if(s != f)
	{
		stream.Append(f, s);
	}

	stream.Append("\n");
}

const char* Log::Get_Text()
{
	return stream.Data();
}
