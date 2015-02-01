#ifndef WINDOWS_CRASH_H
#define WINDOWS_CRASH_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <eh.h>

#include <cstdio>
#include <cstdlib>

enum class Crash
{
	CRT_Terminate,
	CRT_Unexpected,
	CRT_Abort,
	CRT_Invalid_Parameter,
	CRT_Pure_Virtual_Call,
	SEH_Access_Violation,
	SEH_Raise_Exception,
	Thrown_Exception,
	Allocation_Failure,
};

class BaseClass
{
public:
	virtual ~BaseClass() { DoStuff(); }
	void DoStuff() { VirtualDoStuff(); }

	virtual void VirtualDoStuff() = 0;
};

class DerivedClass : public BaseClass
{
public:
	void VirtualDoStuff() {}
};

inline int test_recursive_allocate()
{
	int* pi = new int[0x1FFFFFFF];
	return test_recursive_allocate();
}

inline void test_crash(Crash type)
{
	switch(type)
	{
		case Crash::CRT_Terminate:  terminate();  break;
		case Crash::CRT_Unexpected: unexpected(); break;
		case Crash::CRT_Abort:      abort();      break;
		case Crash::CRT_Invalid_Parameter:
		{
#pragma warning(disable : 6387)
			// warning C6387: 'argument 1' might be '0': this does
			// not adhere to the specification for the function 'printf'
			printf(nullptr);
#pragma warning(default : 6387)
			break;
		}
		case Crash::CRT_Pure_Virtual_Call:
		{
			DerivedClass object;
			break;
		}
		case Crash::SEH_Access_Violation:
		{
			int* p = nullptr;
#pragma warning(disable : 6011)
			// warning C6011: Dereferencing null pointer 'p'
			*p = 0;
#pragma warning(default : 6011)
			break;
		}
		case Crash::SEH_Raise_Exception:
		{
			RaiseException(123, EXCEPTION_NONCONTINUABLE, 0, NULL);
			break;
		}
		case Crash::Thrown_Exception:
		{
			throw "C++ Throw";
			break;
		}
		case Crash::Allocation_Failure:
		{
			test_recursive_allocate();
			break;
		}
	}
}

#endif
