#ifndef FIELD_H
#define FIELD_H

#include "reflection/StrId.h"
#include "core/BitMath.h"

namespace rde
{
struct Type;
class TypeClass;
class TypeRegistry;

namespace FieldFlags
{
enum Enum
{
	HIDDEN			= RDE_BIT(0),
	NO_SERIALIZE	= RDE_BIT(1),
	BOUNDED			= RDE_BIT(2)
};
}

// Optional field information
struct FieldEditInfo
{
	typedef fixed_substring<char, 63>	HelpType;

	FieldEditInfo();

	float		m_limitMin;
	float		m_limitMax;
	HelpType	m_help;
};
RDE_COMPILE_CHECK(sizeof(FieldEditInfo) == 8 + 64);

class Field
{
public:
	Field();
	Field(const StrId& fieldName, uint32 typeId, uint16 offset, const TypeClass* ownerClass,
		const FieldEditInfo* editInfo = 0);
		
	// See also FieldAccessor helper class for more effective way, where
	// offset is not calculated for every access.
	template<typename T>
	void Set(void* object, const TypeClass* objectType, const T& value) const
	{
		*reinterpret_cast<T*>(GetRawDataPtr(object, objectType)) = value;
	}
	template<typename T>
	const T& Get(void* object, const TypeClass* objectType) const
	{
		return *reinterpret_cast<T*>(GetRawDataPtr(object, objectType));
	}

	void* GetRawDataPtr(void* object, const TypeClass* objectType) const;
	void OnPostInit(TypeRegistry& typeReg);

	const TypeClass*		m_ownerClass;
	const Type*				m_type;
	StrId					m_name;
	uint32					m_typeId;
	uint16					m_offset;
	uint16					m_flags;
	const FieldEditInfo*	m_editInfo;	// Optional, may be NULL
};

// Helper class for accessing field data.
class FieldAccessor
{
public:
	FieldAccessor(void* object, const TypeClass* objectType, const Field* field);
	FieldAccessor(void* object, const TypeClass* objectType, const StrId& fieldName);

	bool IsOK() const;

	template<typename T>
	void Set(const T& value)
	{
		*reinterpret_cast<T*>(m_ptr) = value;
	}
	template<typename T>
	const T& Get() const
	{
		return *reinterpret_cast<T*>(m_ptr);
	}
	// Use with care!
	void* GetRawPointer() const
	{
		return m_ptr;
	}

private:
	void Init(void* object, const TypeClass* objectType, const Field* field);
	void*		m_ptr;
};

} // rde

#endif
