#include "core/RdeAssert.h"
#include "core/System.h"
#include "core/win32/Windows.h"
#include <cstdlib>
#include <cstdarg>
#include <psapi.h>
#include <strsafe.h>

#pragma comment(lib, "psapi.lib") // GetModuleBase

namespace
{
// Minidump = process module name with .dmp extension.
void GetMiniDumpFileName(char buffer[_MAX_PATH])
{
	GetModuleBaseName(::GetCurrentProcess(), NULL, buffer, _MAX_PATH);
	char* dotPos = strrchr(buffer, '.');
	if (dotPos)
	{
		RDE_ASSERT(dotPos - buffer <= _MAX_PATH);
		const std::size_t charsLeft = _MAX_PATH - (dotPos - buffer);
		strncpy_s(dotPos, charsLeft, ".dmp", 4);
		dotPos[4] = '\0';
	}
	else
	{
		strncat_s(buffer, _MAX_PATH, ".dmp", 4);
	}
}
} // namespace

namespace rde
{
void Sys::StringFormat(char* outBuf, size_t outBufSize, const char* fmt, ...)
{
	va_list argList;
	va_start(argList, fmt);
	StringCchVPrintf(outBuf, outBufSize, fmt, argList);
	va_end(argList);
}

void Sys::DebugPrint(const char* msg)
{
	::OutputDebugString(msg);
}

void Sys::OnFatalError(const char* msg)
{
	const Sys::ErrorHandlerDelegate& handler = Sys::GetErrorHandler();
	// If special handler exists and it didn't handle our error OR
	// there's no special handler -- go for it.
	if ((handler && !handler(true, msg)) || !handler)
	{
		::MessageBox(NULL, msg, "Fatal Error", MB_OK);

		char moduleName[_MAX_PATH];
		::GetMiniDumpFileName(moduleName);
//		const bool fullDump = ::getenv("RDE_MINIDUMP_FULL") != 0;
//		Debug::WriteMiniDump(moduleName, fullDump, 0);
		abort();
	}
}

void Sys::GetErrorString(char* outString, int maxOutStringLen, DWORD errorOverride)
{
	const DWORD error = (errorOverride == 0 ? ::GetLastError() : errorOverride);
	LPVOID locString(0);
	const DWORD numChars = ::FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&locString,
		maxOutStringLen,
		NULL 
    );
	RDE_ASSERT(numChars <= (DWORD)maxOutStringLen);
	if (numChars == 0)
	{
		::sprintf_s(outString, maxOutStringLen, "Unknown error [0x%X]", error);
	}
	else
		::strcpy_s(outString, maxOutStringLen, (const char*)locString);
}

} // rde
