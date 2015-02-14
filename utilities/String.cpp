#include "String.h"

static inline size_t string_size(const char* s)
{
	const char* start = s;
	while(*s++);
	return s - start - 1;
}

String::String():
	sequence(nullptr),
	capacity(0),
	size(0)
{}

String::String(const String& s):
	sequence(nullptr),
	capacity(0),
	size(0)
{
	Set(s.sequence, s.size);
}

String::String(const char* s):
	sequence(nullptr),
	capacity(0),
	size(0)
{
	Set(s);
}

String::~String()
{
	delete[] sequence;
}

void String::Set(const char* s)
{
	Set(s, string_size(s));
}

void String::Set(const char* s, size_t n)
{
	Reserve(n);
	for(size_t i = 0; i < n; ++i)
		sequence[i] = s[i];
	sequence[n] = '\0';
	size = n;
}

String& String::operator = (const String& s)
{
	if(this != &s) Set(s.sequence, s.size);
	return *this;
}

String& String::operator = (const char* s)
{
	if(sequence != s) Set(s);
	return *this;
}

bool String::Equals(const String& other) const
{
	if(size != other.size) return false;
	for(size_t i = 0; i < size; ++i)
	{
		if(sequence[i] != other.sequence[i])
			return false;
	}
	return true;
}

void String::Append(const char* s, size_t n)
{
	Reserve(size + n);
	for(size_t i = 0; i < n; ++i)
		sequence[size + i] = s[i];
	size += n;
	sequence[size] = '\0';
}

void String::Append(const char* s)
{
	Append(s, string_size(s));
}

void String::Append(const char* first, const char* last)
{
	Append(first, last - first);
}

void String::Append(const String& other)
{
	Append(other.sequence, other.size);
}

void String::Reserve(size_t requested_capacity)
{
	if(requested_capacity <= capacity) return;

	// create new, resized buffer
	size_t new_capacity = requested_capacity | 0xF;
	if(capacity / 2 > new_capacity / 3)
		new_capacity = capacity + capacity / 2;

	char* past_sequence = sequence;
	sequence = new char[new_capacity + 1];

	// copy over to new buffer and delete old
	if(past_sequence != nullptr)
	{
		for(size_t i = 0; i < size; ++i)
			sequence[i] = past_sequence[i];
	}
	delete[] past_sequence;

	// reset things to match new capacity
	sequence[new_capacity] = '\0';
	capacity = new_capacity;
}

void String::Clear()
{
	delete[] sequence;

	sequence = nullptr;
	size = 0;
	capacity = 0;
}

size_t String::Count() const
{
	const char* s = sequence;
	size_t count = 0;
	while(*s) count += (*s++ & 0xc0) != 0x80;
	return count;
}