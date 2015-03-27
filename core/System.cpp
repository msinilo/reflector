#include "core/System.h"

namespace
{
rde::Sys::ErrorHandlerDelegate	s_errorHandler;
}

namespace rde
{
Sys::ErrorHandlerDelegate Sys::SetErrorHandler(const ErrorHandlerDelegate& handler)
{
	ErrorHandlerDelegate prevHandler = s_errorHandler;
	s_errorHandler = handler;
	return prevHandler;
}
const Sys::ErrorHandlerDelegate& Sys::GetErrorHandler()
{
	return s_errorHandler;
}

} // rde
