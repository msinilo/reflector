#if !defined (_MSC_VER)
#	error "MSVC C++ compiler required!"
#endif
#if !defined (CORE_CPU_H)
#	error "Do not include directly, only via core/CPU.h"
#endif

#include <intrin.h>	// __rdtsc()

namespace rde
{
RDE_FORCEINLINE uint64 GetCPUTicks()	{ return __rdtsc(); }
}



