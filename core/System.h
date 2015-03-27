#ifndef CORE_SYSTEM_H
#define CORE_SYSTEM_H

#include "core/Config.h"
#include <external/srutil/delegate.hpp>

namespace rde
{
namespace Sys
{
	// Arguments:
	//	bool - is fatal error,
	//	const char* - error message.
	// Returns true if error was handled by delegate and shouldnt be 
	// further processed, false otherwise.
	typedef srutil::delegate2<bool, bool, const char*>	ErrorHandlerDelegate;

	ErrorHandlerDelegate SetErrorHandler(const ErrorHandlerDelegate& handler);
	const ErrorHandlerDelegate& GetErrorHandler();

	void MemCpy(void* to, const void* from, size_t bytes);
	void MemMove(void* to, const void* from, size_t bytes);
	void MemSet(void* buf, uint8 value, size_t bytes);
	void StringFormat(char* outBuf, size_t outBufSize, const char* fmt, ...);

	void DebugPrint(const char* msg);
	void OnFatalError(const char* msg);
}
}

#if RDE_PLATFORM_WIN32
#	include "win32/Win32System.h"
#else
#	error "Platform not supported"
#endif

#endif // #ifndef CORE_SYSTEM_H
