#include <cstdio>
#include <cstddef>
#include "ReflectionHelpers.h"
#include "reflection/TypeClass.h"
#include "reflection/TypeEnum.h"
#include "reflection/TypeRegistry.h"
#include "io/FileStream.h"
#include "io/StreamReader.h"
#include "rdestl/stack.h"
#include "core/Timer.h"
#include "core/win32/Windows.h"

#define TEST_PERL	1

class Vector3
{
	float	x, y, z;
};
struct Color
{
	float r, g, b;
};
enum EInitVTable
{
	INIT_VTABLE
};
struct SuperBar 
{
	SuperBar(): p(0), psb(0) {}
	explicit SuperBar(EInitVTable) {}
	virtual ~SuperBar() {};

	virtual int VirtualTest()
	{
		return 5;
	}

	static void* Reflection_CreateInstance()
	{
		return new SuperBar();
	}
	static void* Reflection_InitVTable(void* mem)
	{
		return new (mem) SuperBar(INIT_VTABLE);
	}

	unsigned long	i;
	float*			p;
	bool			b;
	signed char		s;
	Color			color;
	SuperBar*		psb;
	typedef rde::vector<int> tVec;
	tVec			v;
};

struct Bar : public SuperBar
{	
	enum
	{
		ARR_MAX = 10
	};
	enum TestEnum
	{
		FIRST	= 0,
		SECOND,
		LAST	= 10
	};	

	virtual ~Bar() {}

	virtual void Foo()
	{
	}
	float	f;
	char	c;
	short	shortArray[ARR_MAX];
	Vector3	position;
	Vector3* porient;
	SuperBar sb;
	SuperBar** pb;
	TestEnum en;
};

struct CircularPtrTest
{
	CircularPtrTest*	ptr;
	int					val;
};

namespace rde
{
	RDE_IMPL_GET_TYPE_NAME(SuperBar);
	RDE_IMPL_GET_TYPE_NAME(CircularPtrTest);
}

#include <dbghelp.h>
#include <cstdlib>

#pragma comment(lib, "dbghelp.lib")

namespace 
{
	unsigned long s_moduleBase(0);

	void GetFileFromPath(const char* path, char* file, int fileNameSize)
	{
		char ext[_MAX_EXT] = { 0 };
		_splitpath_s(path, 0, 0, 0, 0, file, fileNameSize, ext, _MAX_EXT);
		strncat_s(file, fileNameSize, ext, _MAX_EXT);
	}

	BOOL CALLBACK EnumerateModule(PCSTR moduleName, DWORD64 moduleBase, ULONG /*moduleSize*/,
		PVOID /*userContext*/)
	{
		(void)moduleName;
		s_moduleBase = (unsigned long)moduleBase;
		return false;
	}

	void EnumerateModules()
	{
		const HANDLE hCurrentProcess = ::GetCurrentProcess();
		if (::SymInitialize(hCurrentProcess, 0, false))
		{
			char modulePath[_MAX_PATH] = { 0 };
			::GetModuleFileName(::GetModuleHandle(0), modulePath, sizeof(modulePath));

			char moduleFile[_MAX_FNAME] = { 0 };
			GetFileFromPath(modulePath, moduleFile, sizeof(moduleFile));
			::EnumerateLoadedModules64(hCurrentProcess, EnumerateModule, moduleFile);
		}
	}
}

void PrintEnumConstant(const rde::TypeEnum::Constant& e, void*)
{
	printf("%s = %d\n", e.m_name.GetStr(), e.m_value);
}
void PrintField(const rde::Field* field, void*)
{
	printf("Field: %s, offset: %d, type name: %s\n", field->m_name.GetStr(), field->m_offset,
		field->m_type->m_name.GetStr());
}


void TestLoadInPlace(rde::TypeRegistry& typeRegistry)
{
	SuperBar sb;
	sb.i = 5;
	sb.b = false;
	sb.s = -100;
	sb.color.r = 0.7f;
	sb.color.g = 0.2f;
	sb.color.b = 0.55f;
	sb.p = &sb.color.r;
	SuperBar sb2;
	sb2.p = &sb.color.g;
	sb.psb = &sb2;
	sb2.psb = &sb;

	sb.v.push_back(1);
	sb.v.push_back(2);
	
	{
		rde::FileStream ofstream;
		if (!ofstream.Open("test.lip", rde::iosys::AccessMode::WRITE))
			return;

		// Wipe vtable.
		// (the easiest way to test if it'll be properly recreated after load).
		*((void**)&sb) = NULL;

		SaveObject(sb, ofstream, typeRegistry, 1);
		ofstream.Close();
	}
	{
		rde::FileStream ifstream;
		if (!ifstream.Open("test.lip", rde::iosys::AccessMode::READ))
			return;

		rde::uint64 tstart = __rdtsc();
		SuperBar* psb = LoadObject<SuperBar>(ifstream, typeRegistry, 1);
		rde::uint64 loadTime = __rdtsc() - tstart;
		printf("Object loaded in %d ticks\n", loadTime);

		RDE_ASSERT(psb->i == sb.i);
		RDE_ASSERT(psb->b == sb.b);
		RDE_ASSERT(psb->s == sb.s);
		RDE_ASSERT(psb->color.r == sb.color.r);
		RDE_ASSERT(psb->color.g == sb.color.g);
		RDE_ASSERT(psb->color.b == sb.color.b);
		RDE_ASSERT(*psb->p == *sb.p);
		RDE_ASSERT(*psb->p == psb->color.r);
#if !TEST_PERL
		RDE_ASSERT(psb->VirtualTest() == 5);
#endif
		RDE_ASSERT(psb->v.size() == 2);
		RDE_ASSERT(psb->v[0] == 1);
		RDE_ASSERT(psb->v[1] == 2);

		RDE_ASSERT(psb->psb->psb == psb);
		RDE_ASSERT(*psb->psb->p == psb->color.g);

#if !TEST_PERL
		RDE_ASSERT(psb->psb->VirtualTest() == 5);
#endif

		ifstream.Close();
		operator delete(psb);
	}
}

void TestCircular(rde::TypeRegistry& typeRegistry)
{
	CircularPtrTest a;
	CircularPtrTest b;
	CircularPtrTest c;
	a.val = 10;
	b.val = 20;
	c.val = 30;
	a.ptr = &b;
	b.ptr = &c;
	c.ptr = &a;

	{
		rde::FileStream ofstream;
		if (!ofstream.Open("circtest.lip", rde::iosys::AccessMode::WRITE))
			return;
		SaveObject(a, ofstream, typeRegistry, 1);
		ofstream.Close();
	}

	{
		rde::FileStream ifstream;
		if (!ifstream.Open("circtest.lip", rde::iosys::AccessMode::READ))
			return;

		CircularPtrTest* pa = LoadObject<CircularPtrTest>(ifstream, typeRegistry, 1);
		RDE_ASSERT(pa->val == 10);
		CircularPtrTest* pb = pa->ptr;
		RDE_ASSERT(pb->val == 20);
		CircularPtrTest* pc = pb->ptr;
		RDE_ASSERT(pc->val == 30);
		RDE_ASSERT(pc->ptr == pa);
	}
}

int __cdecl main(int, char const *[])
{
	EnumerateModules();

	InitModuleBase(s_moduleBase);

	// Fake call to stop linker from stripping Reflection_InitVTable
	SuperBar::Reflection_InitVTable(0);
	delete SuperBar::Reflection_CreateInstance();

	rde::TypeRegistry typeRegistry;
#if TEST_PERL
	if (!LoadReflectionInfo("perltest.ref", typeRegistry))
	{
		printf("Couldn't load reflection info.\n");
		return 1;
	}
#else
	if (!LoadReflectionInfo("reflectiontest.ref", typeRegistry))
	{
		printf("Couldn't load reflection info.\n");
		return 1;
	}
#endif

	Bar bar;
	const rde::TypeClass* barType = rde::ReflectionTypeCast<rde::TypeClass>(typeRegistry.FindType("Bar"));

	rde::FieldAccessor accessor_f(&bar, barType, "f");
	accessor_f.Set(5.f);
	RDE_ASSERT(bar.f == 5.f);
	bar.Foo();

	const rde::Field* field = barType->FindField("i");
	field->Set(&bar, barType, 10);
	RDE_ASSERT(bar.i == 10);

	rde::FieldAccessor accessorArray(&bar, barType, "shortArray");
	short* pArray = (short*)accessorArray.GetRawPointer();
	pArray[0] = 0;
	pArray[1] = 1;
	pArray[2] = 2;
	RDE_ASSERT(bar.shortArray[0] == 0 && bar.shortArray[1] == 1 && bar.shortArray[2] == 2);

	// Print all pointers of Bar.
	barType->EnumerateFields(PrintField, rde::ReflectionType::POINTER);

	const rde::TypeEnum* enumType = rde::ReflectionTypeCast<rde::TypeEnum>(typeRegistry.FindType("Bar::TestEnum"));
	if (enumType)
	{
		printf("enum %s\n", enumType->m_name.GetStr());
		enumType->EnumerateConstants(PrintEnumConstant);
	}

	TestLoadInPlace(typeRegistry);

#if !TEST_PERL
	SuperBar* psb = typeRegistry.CreateInstance<SuperBar>("SuperBar");
	RDE_ASSERT(psb->VirtualTest() == 5);
#endif

	TestCircular(typeRegistry);

	return 0;
}
