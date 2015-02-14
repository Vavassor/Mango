#ifndef LOGGING_H

namespace Log {

enum class Level
{
	Error,
	Info,
	Debug,
};

void Clear_File();
void Inc_Time();
void Output();
void Add(Level level, const char* format, ...);
const char* Get_Text();

} // namespace Log

#if defined(_DEBUG)
#define DEBUG_LOG(format, ...) Log::Add(Log::Level::Debug, (format), ##__VA_ARGS__)
#else
#define DEBUG_LOG(format, ...) // no operation
#endif

#define LOG_ISSUE(format, ...) Log::Add(Log::Level::Error, (format), ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::Add(Log::Level::Info, (format), ##__VA_ARGS__)

#define LOGGING_H
#endif
