#include "core/MemManager.h"
#include <new>

namespace
{
	struct MemManagerInitializer
	{
		static void __cdecl ExitMem()
		{
			rde::MemManager::Exit();
		}
		MemManagerInitializer()
		{
			::atexit(&ExitMem);
		}
	} s_memManagerInitializer;

	rde::MemManager::OnAlloc	onAllocHandler(0);
	rde::MemManager::OnFree		onFreeHandler(0);
}

#if 1
//-----------------------------------------------------------------------------
void* __cdecl operator new(size_t bytes) 
{
	return rde::MemManager::Alloc(bytes);
}
void* __cdecl operator new[](size_t bytes) 
{
	return rde::MemManager::Alloc(bytes);
}
void* __cdecl operator new(size_t bytes, const std::nothrow_t&) throw() 
{
	return rde::MemManager::Alloc(bytes);
} 
void* __cdecl operator new[](size_t bytes, const std::nothrow_t&) throw() 
{
	return rde::MemManager::Alloc(bytes);
} 

//-----------------------------------------------------------------------------
void __cdecl operator delete(void* ptr) throw()
{
	rde::MemManager::Free(ptr);
}
void __cdecl operator delete[](void* ptr) throw()
{
	rde::MemManager::Free(ptr);
}
#endif

namespace rde
{
#ifdef RDE_CORE_MEMMGR_HEADER
#	include RDE_CORE_MEMMGR_HEADER
#else

static long s_numAllocations(0);

//-----------------------------------------------------------------------------
void* MemManager::Alloc(size_t bytes, const char* tag)
{
	void* ptr = ::malloc(bytes);
	if (onAllocHandler)
		onAllocHandler(ptr, bytes, tag);
	return ptr;
}

//-----------------------------------------------------------------------------
void MemManager::Free(void* ptr) throw()
{
	if (ptr != 0)
	{
		::free(ptr);
		if (onFreeHandler)
			onFreeHandler(ptr);
	}
}

//-----------------------------------------------------------------------------
void MemManager::BeginFrame()
{
}

//-----------------------------------------------------------------------------
void MemManager::EndFrame()
{
}

//-----------------------------------------------------------------------------
void MemManager::SetOnAllocHandler(OnAlloc handler)
{
	onAllocHandler = handler;
}

//-----------------------------------------------------------------------------
void MemManager::SetOnFreeHandler(OnFree handler)
{
	onFreeHandler = handler;
}

//-----------------------------------------------------------------------------
void MemManager::Exit()
{
	int tits = 1;
	sizeof(tits);
}

#endif // #RDE_CORE_MEMMGR_HEADER 

} // rde
//-----------------------------------------------------------------------------


