#ifndef CORE_MEMMANAGER_H
#define CORE_MEMMANAGER_H

namespace rde
{
// Memory manager.
struct MemManager
{
	// Handler functions called on every allocation/every free.
	typedef void (*OnAlloc)(const void* address, size_t bytes, const char* tag);
	typedef void (*OnFree)(const void* address);

	static void* Alloc(size_t bytes, const char* tag = 0);
    static void Free(void* ptr) throw();

	// For more detailed memory profiling.
	static void BeginFrame();
	static void EndFrame();

	static void SetOnAllocHandler(OnAlloc handler);
	static void SetOnFreeHandler(OnFree handler);
    
    // Deinitializes memory manager. It's forbidden to call Alloc/Fre
    // after this function has been executed.
	// It's automatically called from atexit().
    static void Exit();
};

} // rde

//-----------------------------------------------------------------------------
#endif // #ifndef CORE_MEMMANAGER_H
