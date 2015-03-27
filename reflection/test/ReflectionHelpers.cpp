#include "reflection/TypeClass.h"
#include "reflection/TypeEnum.h"
#include "reflection/TypeRegistry.h"
#include "io/FileStream.h"
#include "io/StreamReader.h"
#include "rdestl/stack.h"
#include "core/OwnedPtr.h"

namespace
{
size_t	s_moduleBase(0);

#define DBG_VERBOSITY_LEVEL			0

#if DBG_VERBOSITY_LEVEL == 0
#	define DBGPRINTF
#	define DBGPRINTF2
#elif DBG_VERBOSITY_LEVEL == 1
#	define DBGPRINTF	rde::Console::Printf
#	define DBGPRINTF2
#else
#	define DBGPRINTF	rde::Console::Printf
#	define DBGPRINTF2	rde::Console::Printf
#endif

#pragma pack(push, 1)
struct FieldData
{
	rde::uint32	typeId;
	rde::uint16	offset;
	rde::uint16 flags;
	rde::uint16 fieldEditIndex;
};
#pragma pack(pop)

void LoadFields(rde::StreamReader& sr, rde::TypeClass& tc, rde::FieldEditInfo* fieldInfos)
{
	const int numFields = sr.ReadInt32();
	char fieldNameBuffer[128];
	static const rde::uint16 INVALID_INDEX = 0xFFFF;
	for (int i = 0; i < numFields; ++i)
	{
		FieldData fieldData;
		sr.Read(&fieldData, sizeof(fieldData));
		sr.ReadASCIIZ(fieldNameBuffer, sizeof(fieldNameBuffer));
		rde::FieldEditInfo* info = (fieldData.fieldEditIndex == INVALID_INDEX ? 
			0 : fieldInfos + fieldData.fieldEditIndex);
		rde::Field field(fieldNameBuffer, fieldData.typeId, fieldData.offset, &tc, info);
		field.m_flags = fieldData.flags;
		tc.AddField(field);
	}
}
void LoadReflectionInfo(rde::Stream& stream, rde::TypeRegistry& typeRegistry)
{
	rde::StreamReader sr(&stream);
	const int numTypes = sr.ReadInt32();
	const int numFieldInfos = sr.ReadInt32();

	rde::FieldEditInfo* fieldInfos(0);
	if (numFieldInfos != 0)
	{
		fieldInfos = new rde::FieldEditInfo[numFieldInfos];
		char helpBuffer[64];
		for (int i = 0; i < numFieldInfos; ++i)
		{
			sr.Read(&fieldInfos[i].m_limitMin, sizeof(fieldInfos[i].m_limitMin) * 2); // min + max
			sr.ReadASCIIZ(helpBuffer, sizeof(helpBuffer));
			fieldInfos[i].m_help = helpBuffer;
		}
	}

	char nameBuffer[512];
	for (int i = 0; i < numTypes; ++i)
	{
		sr.ReadASCIIZ(nameBuffer, sizeof(nameBuffer));
		const rde::uint32 typeSize = sr.ReadInt32();
		const int reflectionType = sr.ReadInt32();

		rde::Type* newType(0);
		if (reflectionType == rde::ReflectionType::CLASS)
		{
			const rde::uint32 baseClassId = sr.ReadInt32();
			const rde::uint16 baseClassOffset = sr.ReadInt16();

			size_t createInstanceFuncAddress = sr.ReadInt32();
			if (createInstanceFuncAddress != 0)
				createInstanceFuncAddress += s_moduleBase;
			rde::TypeClass::FnCreateInstance pfnCreateInstance = (rde::TypeClass::FnCreateInstance)createInstanceFuncAddress;

			size_t initVTableFuncAddress = sr.ReadInt32();
			if (initVTableFuncAddress != 0)
				initVTableFuncAddress += s_moduleBase;
			rde::TypeClass::FnInitVTable pfnInitVTable = (rde::TypeClass::FnInitVTable)initVTableFuncAddress;

			rde::TypeClass* tc = new rde::TypeClass(typeSize, nameBuffer, pfnCreateInstance, pfnInitVTable, 
				baseClassId, baseClassOffset);
			LoadFields(sr, *tc, fieldInfos);
			newType = tc;
		}
		else if (reflectionType == rde::ReflectionType::ENUM)
		{
			const int numEnumElements = sr.ReadInt32();
			char enumeratorNameBuffer[128];
			rde::TypeEnum* te = new rde::TypeEnum(typeSize, nameBuffer);
			for (int i = 0; i < numEnumElements; ++i)
			{
				sr.ReadASCIIZ(enumeratorNameBuffer, sizeof(enumeratorNameBuffer));
				const int enumeratorValue = sr.ReadInt32();
				rde::TypeEnum::Constant enumConstant(enumeratorNameBuffer, enumeratorValue);
				te->AddConstant(enumConstant);
			}
			newType = te;
		}
		else if (reflectionType == rde::ReflectionType::POINTER)
		{
			const rde::uint32 pointedTypeId = sr.ReadInt32();
			newType = new rde::TypePointer(typeSize, nameBuffer, pointedTypeId);
		}
		else if (reflectionType == rde::ReflectionType::ARRAY)
		{
			const rde::uint32 containedTypeId = sr.ReadInt32();
			const int numElements = sr.ReadInt32();
			newType = new rde::TypeArray(typeSize, nameBuffer, containedTypeId, numElements);
		}
		else
		{
			RDE_ASSERT(!"Invalid reflection type");
		}
		if (newType)
			typeRegistry.AddType(newType);
	}
	if (fieldInfos)
	{
		typeRegistry.AddFieldEditInfos(fieldInfos);
	}
}

//-----------------------------------------------------------------------------------------------------
// Load-in-place test system

struct ObjectHeader
{
	rde::uint32	typeTag;
	rde::uint32	size;
	rde::uint32 version;
	rde::uint16	numPointerFixups;
};
struct PointerFixupEntry
{
	PointerFixupEntry(): m_pointerOffset(0), m_pointerValueOffset(0), m_typeTag(0) {}

	rde::uint32	m_pointerOffset;
	rde::uint32	m_pointerValueOffset;
	// 0 if no need to patch vtable.
	rde::uint32	m_typeTag;	
};
struct RawFieldInfo
{
	void**		m_mem;
	size_t		m_size;
	int			m_fixupIndex;
#if DBG_VERBOSITY_LEVEL > 0
	rde::StrId	m_name;
	int			m_nestLevel;
#endif
};

typedef rde::fixed_vector<PointerFixupEntry, 16, true> PointerFixups;
typedef rde::fixed_vector<RawFieldInfo, 16, true> RawFields;
typedef rde::vector<const rde::Field*> Fields;

struct CollectContext
{
	struct ObjectStackEntry
	{
		void*					m_obj;
		const rde::TypeClass*	m_objType;
		rde::uint32				m_pointerOffset;
	};
	typedef rde::stack<ObjectStackEntry, rde::allocator, 
		rde::fixed_vector<ObjectStackEntry, 16, true> >	ObjectStack;

	CollectContext(): m_dataSize(0), m_pointerValueOffset(0), m_typeRegistry(0) {}

	PointerFixups		m_fixups;
	ObjectStack			m_objectStack;
	RawFields			m_fields;
	rde::uint32			m_dataSize;
	rde::uint32			m_pointerValueOffset;
	rde::TypeRegistry*	m_typeRegistry;
};
void CollectMembers(const rde::Field* field, void* userData);

// Cannot have non-serializable fields.
bool IsLoadInPlaceCompatible(const rde::TypeClass* tc)
{
	const int numFields = tc->GetNumFields();
	for (int i = 0; i < numFields; ++i)
	{
		const rde::Field* field = tc->GetField(i);
		if (field->m_flags & rde::FieldFlags::NO_SERIALIZE)
			return false;
	}
	return true;
}

void CollectPointer(void** rawFieldMem, CollectContext* context, const rde::TypePointer* tp,
					rde::uint32 fieldOffset, const char* fieldName)
{
	RDE_ASSERT(!context->m_objectStack.empty());
	CollectContext::ObjectStackEntry& topEntry = context->m_objectStack.top();
	const rde::uint32 currentPointerOffset = topEntry.m_pointerOffset;

	// NULL pointer, ignore, no need to patch, it'll be written directly as 0.
	if (*rawFieldMem == 0)
		return;

	// Pointer already processed?
	int iField = -1;
	for (iField = 0; iField < context->m_fields.size(); ++iField)
	{
		if (*context->m_fields[iField].m_mem == *rawFieldMem)
			break;
	}
	const bool ptrAlreadyFound(iField != context->m_fields.size());

	const rde::uint32 currentPointerValueOffset = context->m_pointerValueOffset;
	PointerFixupEntry ptrFixup;
	const rde::Type* pointedType = context->m_typeRegistry->FindType(tp->m_pointedTypeId);
	const rde::TypeClass* tc = rde::ReflectionTypeCast<rde::TypeClass>(pointedType);
	if (ptrAlreadyFound)
	{
		ptrFixup.m_pointerOffset = currentPointerOffset + fieldOffset;
		const int iFixup = context->m_fields[iField].m_fixupIndex;
		ptrFixup.m_pointerValueOffset = context->m_fixups[iFixup].m_pointerValueOffset;
	}
	else
	{
		ptrFixup.m_pointerOffset = currentPointerOffset + fieldOffset;
		ptrFixup.m_pointerValueOffset = currentPointerValueOffset;
	}
	if (tc && tc->HasVTable())
	{
		ptrFixup.m_typeTag = tc->m_name.GetId();
	}
	DBGPRINTF("Field: %s, offset: %d, fixup offset: %d\n", 
		fieldName, ptrFixup.m_pointerOffset, ptrFixup.m_pointerValueOffset);

	context->m_fixups.push_back(ptrFixup);
	if (!ptrAlreadyFound)
	{
		RawFieldInfo fieldInfo = 
		{ 
			rawFieldMem,
			pointedType->m_size, 
			context->m_fixups.size() - 1 
#if DBG_VERBOSITY_LEVEL > 0
			, fieldName, context->m_objectStack.size()
#endif
		};
		context->m_fields.push_back(fieldInfo);
		context->m_dataSize += pointedType->m_size;
		context->m_pointerValueOffset += pointedType->m_size;
	}
	// Recurse down if pointing to class.
	if (tc && !ptrAlreadyFound)
	{
		// Next type will be saved after what we've saved so far (so new object offset == dataSize).
		CollectContext::ObjectStackEntry newEntry = { *rawFieldMem, tc, context->m_dataSize }; 
		context->m_objectStack.push(newEntry);

		tc->EnumerateFields(CollectMembers, rde::ReflectionType::POINTER | rde::ReflectionType::CLASS, context);
		context->m_objectStack.pop();
	}
}
void CollectClass(void* rawPointer, CollectContext* context, const rde::TypeClass* tc, rde::uint32 fieldOffset);
void CollectPointers_Vector(CollectContext* context, const rde::TypeClass* tc)
{
	// Two fields: begin & end.
	// We need to serialize whole array between those.
	CollectContext::ObjectStackEntry& topEntry = context->m_objectStack.top();

	// That's a little bit risky, we probably should access fields by name, but
	// rde::vector is 'fixed' and this way it's quicker.
	const rde::Field* fieldBegin = tc->GetField(0);
	const rde::Field* fieldEnd = tc->GetField(1);

	// Let's calculate how many bytes do we need to save.
	rde::FieldAccessor accessorBegin(topEntry.m_obj, topEntry.m_objType, fieldBegin);
	rde::FieldAccessor accessorEnd(topEntry.m_obj, topEntry.m_objType, fieldEnd);
	const rde::uint8** ppBegin = (const rde::uint8**)accessorBegin.GetRawPointer();
	const rde::uint8** ppEnd = (const rde::uint8**)accessorEnd.GetRawPointer();
	const rde::uint32 numBytes = (rde::uint32)(*ppEnd - *ppBegin);
	if (numBytes == 0)
		return;

	RawFieldInfo fieldInfo = 
	{ 
		(void**)ppBegin, numBytes, -1
#if DBG_VERBOSITY_LEVEL > 0
		, "rde::vector", context->m_objectStack.size()
#endif
	};
	context->m_fields.push_back(fieldInfo);
	context->m_dataSize += numBytes;

	// Fix-ups for begin pointer.
	const rde::uint32 currentPointerOffset = topEntry.m_pointerOffset;
	const rde::uint32 currentPointerValueOffset = context->m_pointerValueOffset;
	PointerFixupEntry ptrFixup;
	ptrFixup.m_pointerOffset = currentPointerOffset + fieldBegin->m_offset;
	ptrFixup.m_pointerValueOffset = currentPointerValueOffset;
	context->m_fixups.push_back(ptrFixup);
//	DBGPRINTF("Vector field: %s, begin offset: %d, fixup offset: %d\n", field->m_name.GetStr(),
//		ptrFixup.m_pointerOffset, ptrFixup.m_pointerValueOffset);

	// Fixup for end pointer.
	ptrFixup.m_pointerOffset = currentPointerOffset + fieldEnd->m_offset;
	ptrFixup.m_pointerValueOffset = currentPointerValueOffset + numBytes;
	context->m_fixups.push_back(ptrFixup);
//	DBGPRINTF("Vector field: %s, end offset: %d, fixup offset: %d\n", field->m_name.GetStr(),
//		ptrFixup.m_pointerOffset, ptrFixup.m_pointerValueOffset);

	// That's how much raw data we actually save.
	context->m_pointerValueOffset += numBytes;

	// Now fixups for vector elements if needed.
	const rde::TypePointer* tp = rde::SafeReflectionCast<rde::TypePointer>(fieldBegin->m_type);
	const rde::Type* pointedType = context->m_typeRegistry->FindType(tp->m_pointedTypeId);
	if (pointedType->m_reflectionType == rde::ReflectionType::POINTER)
	{
		// Collection of pointers.
		RDE_ASSERT(numBytes % sizeof(void*) == 0);
		const int size = numBytes / sizeof(void*);
		const rde::uint8* pBegin = *ppBegin;
		// Fake object (starts where vector contents are saved).
		CollectContext::ObjectStackEntry newEntry = { 0, 0, currentPointerValueOffset };
		context->m_objectStack.push(newEntry);
		for (int i = 0; i < size; ++i)
		{
			CollectPointer((void**)pBegin, context, static_cast<const rde::TypePointer*>(pointedType), 
				i * sizeof(void*), "vecelem");
			pBegin += sizeof(void*);
		}
		context->m_objectStack.pop();
	}
	else if (pointedType->m_reflectionType == rde::ReflectionType::CLASS)
	{
		// Collection of classes. Need to generate fix-ups for them.
		RDE_ASSERT(numBytes % pointedType->m_size == 0);
		const int numElements = numBytes / pointedType->m_size;
		const rde::uint8* pBegin = *ppBegin;
		// Fake object (starts where vector contents are saved).
		CollectContext::ObjectStackEntry newEntry = { 0, 0, currentPointerValueOffset };
		context->m_objectStack.push(newEntry);
		for (int i = 0; i < numElements; ++i)
		{
			CollectClass((void*)pBegin, context, static_cast<const rde::TypeClass*>(pointedType), 
				i * pointedType->m_size);
			pBegin += pointedType->m_size;
		}
		context->m_objectStack.pop();
	}
}

void CollectClass(void* rawPointer, CollectContext* context, const rde::TypeClass* tc, rde::uint32 fieldOffset)
{
	CollectContext::ObjectStackEntry& topEntry = context->m_objectStack.top();
	const rde::uint32 currentPointerOffset = topEntry.m_pointerOffset;
	RDE_ASSERT(IsLoadInPlaceCompatible(tc));
	CollectContext::ObjectStackEntry newEntry = 
	{ 
		rawPointer, tc, 
		currentPointerOffset + fieldOffset 
	};
	context->m_objectStack.push(newEntry);
	// Special case A: rde::vector.
	static const char* rdeVectorName = "vector<";
	static const long rdeVectorNameLen = rde::strlen(rdeVectorName);

	const char* tcName = tc->m_name.GetStr();
	// -1 -> 0, 0->1 etc. (so that -1 doesn't require further fixing).
	int colonPos = rde::find_index_of(tcName, ':') + 1;
	if (colonPos > 0)
	{
		++colonPos;
	}
	if (rde::strcompare(tcName + colonPos, rdeVectorName, rdeVectorNameLen) == 0)
	{
		CollectPointers_Vector(context, tc);
	}
	else // 'ordinary' class, enumerate all pointers (TODO: handle other special classes!)
	{
		tc->EnumerateFields(CollectMembers, rde::ReflectionType::POINTER | rde::ReflectionType::CLASS, context);
	}
	context->m_objectStack.pop();
}

void CollectClass(const rde::Field* field, CollectContext* context)
{
	CollectContext::ObjectStackEntry& topEntry = context->m_objectStack.top();
	rde::FieldAccessor fieldAccessor(topEntry.m_obj, topEntry.m_objType, field);
	const rde::TypeClass* tc = static_cast<const rde::TypeClass*>(field->m_type);
	CollectClass(fieldAccessor.GetRawPointer(), context, tc, field->m_offset);
}
void CollectPointer(const rde::Field* field, CollectContext* context)
{
	CollectContext::ObjectStackEntry& topEntry = context->m_objectStack.top();
	rde::FieldAccessor fieldAccessor(topEntry.m_obj, topEntry.m_objType, field);

	void** rawFieldMem = (void**)fieldAccessor.GetRawPointer();
	CollectPointer(rawFieldMem, context, static_cast<const rde::TypePointer*>(field->m_type), field->m_offset,
		field->m_name.GetStr());
}

void CollectMembers(const rde::Field* field, void* userData)
{
	CollectContext* context = (CollectContext*)userData;

	// Special case: field is some other class.
	if (field->m_type->m_reflectionType == rde::ReflectionType::CLASS)
	{
		CollectClass(field, context);
	}
	else	// ptr
	{
		CollectPointer(field, context);
	}
}

} // namespace

void InitModuleBase(size_t moduleBase)
{
	s_moduleBase = moduleBase;
}

bool LoadReflectionInfo(const char* fileName, rde::TypeRegistry& typeRegistry)
{
	rde::FileStream fstream;
	if (!fstream.Open(fileName, rde::iosys::AccessMode::READ))
		return false;

	LoadReflectionInfo(fstream, typeRegistry);
	typeRegistry.PostInit();
	return true;
}

void* LoadObjectImpl(rde::Stream& stream, rde::TypeRegistry& typeRegistry, rde::uint32 version)
{
	ObjectHeader objectHeader;
	stream.Read(&objectHeader, sizeof(objectHeader));

	// Version mismatch
	if (version != 0 && version != objectHeader.version)
		return 0;

	const rde::TypeClass* type = 
		static_cast<const rde::TypeClass*>(typeRegistry.FindType(objectHeader.typeTag));
	PointerFixups pointerFixups;
	if (objectHeader.numPointerFixups > 0)
	{
		pointerFixups.resize(objectHeader.numPointerFixups);
		stream.Read(pointerFixups.begin(), objectHeader.numPointerFixups * sizeof(PointerFixupEntry));
	}
	void* objectMem = operator new(objectHeader.size);
	stream.Read(objectMem, objectHeader.size);
	type->InitVTable(objectMem);

	rde::uint8* objectMem8 = static_cast<rde::uint8*>(objectMem);
	for (PointerFixups::const_iterator it = pointerFixups.begin(); it != pointerFixups.end(); ++it)
	{
		rde::uint8* pptr = objectMem8 + it->m_pointerOffset;
		void* patchedMem = objectMem8 + it->m_pointerValueOffset;
		*reinterpret_cast<void**>(pptr) = patchedMem;

		if (it->m_typeTag != 0)
		{
			const rde::TypeClass* fieldType = 
				static_cast<const rde::TypeClass*>(typeRegistry.FindType(it->m_typeTag));
			// We already initialized vtable for 'main' object.
			if (patchedMem != objectMem)
				fieldType->InitVTable(patchedMem);
		}
	}
	return objectMem;
}

// Rough layout:
//	- header
//	- pointer fixups
//	- main object
//	- objects referenced in main object (raw mem).
// Pointer fixups are in format:
//	- offset of pointer to fix-up (from start of main object),
//	- offset of memory to set pointer to

void SaveObjectImpl(const void* obj, const rde::StrId& typeName, rde::Stream& stream, 
					rde::TypeRegistry& typeRegistry, rde::uint32 version)
{
	const rde::TypeClass* type = rde::ReflectionTypeCast<rde::TypeClass>(typeRegistry.FindType(typeName));
	RDE_ASSERT(IsLoadInPlaceCompatible(type));

	ObjectHeader objectHeader;
	objectHeader.typeTag = type->m_name.GetId();
	objectHeader.version = version;
 
	// Initialize context.
	CollectContext collectContext;
	// We first save 'obj', skip it here (pointer data is saved after main object).
	collectContext.m_pointerValueOffset = type->m_size;
	CollectContext::ObjectStackEntry startEntry = { (void*)obj, type, 0 };
	collectContext.m_objectStack.push(startEntry);
	collectContext.m_typeRegistry = &typeRegistry;
	// Treat us as a field as well (in case someone keeps a reference to us).
	RawFieldInfo startField = 
	{ 
		(void**)&obj, type->m_size, 0 
#if DBG_VERBOSITY_LEVEL > 0
		, typeName, 0
#endif
	};
	collectContext.m_fields.push_back(startField);
	// Initial fix-up (0, 0) for main object.
	PointerFixupEntry ptrFixup;
	collectContext.m_fixups.push_back(ptrFixup);
	type->EnumerateFields(CollectMembers, rde::ReflectionType::POINTER | rde::ReflectionType::CLASS, &collectContext);
	// Fix size, couldn't do it earlier, because we used this as object offset, so it had to be zero.
	collectContext.m_dataSize += type->m_size;

	// Write object header.
	objectHeader.size = collectContext.m_dataSize;
	RDE_ASSERT(collectContext.m_fixups.size() < 65536);
	objectHeader.numPointerFixups = (rde::uint16)collectContext.m_fixups.size() - 1;
	stream.Write(&objectHeader, sizeof(ObjectHeader));

	// Write fixups
	// Skip initial fixup, it's always 0, 0
	if (objectHeader.numPointerFixups > 0)
		stream.Write(collectContext.m_fixups.begin() + 1, objectHeader.numPointerFixups * sizeof(PointerFixupEntry));

	// Raw object memory (main obj + ptr fields).
#if DBG_VERBOSITY_LEVEL > 0
	const int objectMemStart = stream.GetPosition();
#endif
	for (RawFields::iterator it = collectContext.m_fields.begin(); it != collectContext.m_fields.end(); ++it)
	{
#if DBG_VERBOSITY_LEVEL > 0
		DBGPRINTF2("%*s%d: %s [%d byte(s)]\n", it->m_nestLevel, "", 
			stream.GetPosition() - objectMemStart, it->m_name.GetStr(), it->m_size);
#endif
		stream.Write(*it->m_mem, (long)it->m_size);
	}
}
