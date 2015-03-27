#ifndef REFLECTIONHELPERS_H
#define REFLECTIONHELPERS_H

#include "core/Config.h"

namespace rde
{
class StrId;
class Stream;
class TypeRegistry;
}

void InitModuleBase(size_t moduleBase);
bool LoadReflectionInfo(const char* fileName, rde::TypeRegistry& typeRegistry);
void SaveObjectImpl(const void* obj, const rde::StrId& typeName, rde::Stream& stream, 
					rde::TypeRegistry& typeRegistry, rde::uint32 version);
void* LoadObjectImpl(rde::Stream& stream, rde::TypeRegistry& typeRegistry, rde::uint32 version);

template<typename T>
void SaveObject(const T& obj, rde::Stream& stream, rde::TypeRegistry& typeRegistry, rde::uint32 version)
{
	SaveObjectImpl(&obj, rde::GetTypeName<T>(), stream, typeRegistry, version);
}

template<typename T>
T* LoadObject(rde::Stream& stream, rde::TypeRegistry& typeRegistry, rde::uint32 version)
{
	return static_cast<T*>(LoadObjectImpl(stream, typeRegistry, version));
}

#endif
