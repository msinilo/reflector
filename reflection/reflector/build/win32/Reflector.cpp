#include "reflection/TypeClass.h"
#include "reflection/TypeRegistry.h"
#include "io/ChunkStreamReader.h"
#include "io/ChunkStreamWriter.h"
#include "io/FileStream.h"
#include "rdestl/hash_map.h"
#include "rdestl/vector.h"
#include "core/RefCounted.h"
#include "core/RefPtr.h"
#include <dia2.h>
#include <cstdio>
#include <string>

#define LOAD_TEST	0

namespace
{
	struct FieldDescriptor;
	struct TypeDescriptor : public rde::RefCounted<TypeDescriptor>
	{
		TypeDescriptor(): m_size(0), m_baseClassOffset(0), m_type(0) {}
		~TypeDescriptor();

		bool HasField(const rde::StrId& name) const;
		bool AddField(FieldDescriptor*);

		void WriteFields(rde::StreamWriter& sw) const;
		void WriteTypeInfo(rde::StreamWriter& sw) const
		{
			sw.WriteASCIIZ(m_name.GetStr());
			sw.WriteInt32((long)m_size);
			sw.WriteInt32(m_reflectionType);
			sw.WriteInt32(m_numElements);
			sw.WriteASCIIZ(m_baseClassName.GetStr());
			sw.WriteInt32(m_baseClassOffset);
			WriteFields(sw);
		}

		typedef rde::vector<FieldDescriptor*>	Fields;
		rde::StrId					m_name;
		size_t						m_size;
		rde::ReflectionType::Enum	m_reflectionType;
		rde::StrId					m_baseClassName;
		rde::uint32_t				m_baseClassOffset;
		rde::uint32_t				m_numElements;	// array
		Fields						m_fields;
		rde::TypeClass*				m_type;
	};
	struct FieldDescriptor
	{
		FieldDescriptor(): m_offset(0) {}

		void Write(rde::StreamWriter& sw) const
		{
			sw.WriteASCIIZ(m_typeName.GetStr());
			sw.WriteInt32(m_offset);
			sw.WriteASCIIZ(m_name.GetStr());
		}

		rde::StrId	m_typeName;
		int			m_offset;
		rde::StrId	m_name;
	};

	TypeDescriptor::~TypeDescriptor()
	{
		for (int i = 0; i < m_fields.size(); ++i)
			delete m_fields[i];
		m_fields.clear();
	}
	bool TypeDescriptor::HasField(const rde::StrId& name) const
	{
		for (int i = 0; i < m_fields.size(); ++i)
		{
			if (name == m_fields[i]->m_name)
				return true;
		}
		return false;
	}
	bool TypeDescriptor::AddField(FieldDescriptor* field)
	{
		if (HasField(field->m_name))
			return false;
		m_fields.push_back(field);
		return true;
	}
	void TypeDescriptor::WriteFields(rde::StreamWriter& sw) const
	{
		sw.WriteInt32(m_fields.size());
		for (int i = 0; i < m_fields.size(); ++i)
			m_fields[i]->Write(sw);
	}

	typedef rde::RefPtr<TypeDescriptor>					TypeDescPtr;
	typedef rde::hash_map<rde::uint32_t, TypeDescPtr>	TypeMap;
	TypeMap	s_typeDescriptors;

	TypeDescriptor* FindTypeDescriptor(const rde::StrId& name)
	{
		TypeMap::iterator it = s_typeDescriptors.find(name.GetId());
		return it == s_typeDescriptors.end() ? 0 : it->second.GetPtr();
	}
	TypeDescriptor* AddTypeDescriptor(const rde::StrId& name, rde::ReflectionType::Enum reflectionType,
		size_t typeSize)
	{
		TypeDescriptor* desc = FindTypeDescriptor(name);
		if (desc == 0)
		{
			desc = new TypeDescriptor();
			desc->m_name = name;
			desc->m_reflectionType = reflectionType;
			desc->m_size = typeSize;
			s_typeDescriptors.insert(rde::make_pair(name.GetId(), TypeDescPtr(desc)));
		}
		return desc;
	}

	const rde::Type* RegisterFieldType(const rde::StrId& typeName, rde::TypeRegistry& typeRegistry)
	{
		const rde::Type* t = typeRegistry.FindType(typeName);
		if (t == 0)	// Register type if not found yet.
		{
			printf("Needs to register: %s\n", typeName.GetStr());
		}
		return t;
	}
	void RegisterTypes(rde::TypeRegistry& typeRegistry)
	{
		for (TypeMap::iterator it = s_typeDescriptors.begin(); it != s_typeDescriptors.end(); ++it)
		{
			TypeDescPtr& desc = it->second;
			rde::Type* newType(0);
			if (desc->m_reflectionType == rde::ReflectionType::POINTER)
			{
				newType = new rde::TypePointer(desc->m_size, desc->m_name.GetStr());
			}
			else if (desc->m_reflectionType == rde::ReflectionType::ARRAY)
			{
				newType = new rde::TypeArray(desc->m_size, desc->m_name.GetStr(), desc->m_numElements);
			}
			if (newType)
				typeRegistry.AddType(newType);
		}
	}
	void RegisterClassTypes(rde::TypeRegistry& typeRegistry)
	{
		RegisterTypes(typeRegistry);
		// First -- register all UDTs
		for (TypeMap::iterator it = s_typeDescriptors.begin(); it != s_typeDescriptors.end(); ++it)
		{
			TypeDescPtr& desc = it->second;
			if (desc->m_reflectionType != rde::ReflectionType::CLASS)
				continue;
			rde::TypeClass* newType = new rde::TypeClass(desc->m_size, desc->m_name.GetStr(), desc->m_baseClassName, 
				(rde::uint16_t)desc->m_baseClassOffset);
			typeRegistry.AddType(newType);
			desc->m_type = newType;
		}
		// Now, prepare their fields.
		for (TypeMap::iterator it = s_typeDescriptors.begin(); it != s_typeDescriptors.end(); ++it)
		{
			const TypeDescPtr& desc = it->second;
			rde::TypeClass* newType = desc->m_type;
			for (int i = 0; i < desc->m_fields.size(); ++i)
			{
				const FieldDescriptor& fieldDesc = *desc->m_fields[i];
				rde::Field field;
				field.m_type = RegisterFieldType(fieldDesc.m_typeName, typeRegistry);
				field.m_ownerClass = newType;
				field.m_offset = (rde::uint16_t)fieldDesc.m_offset;
				field.m_name = fieldDesc.m_name;
				field.m_flags = 0x0;	// TO BE IMPLEMENTED
				newType->AddField(field);
			}

			// Finalize.
			typeRegistry.PostInit();
		}
	}

	void SaveFields(rde::ChunkStreamWriter& csw, const TypeDescriptor& typeDesc)
	{
		csw.WriteInt32(typeDesc.m_fields.size());
		if (typeDesc.m_fields.empty())
			return;
		csw.WriteASCIIZ(typeDesc.m_name.GetStr());
		for (TypeDescriptor::Fields::const_iterator it = typeDesc.m_fields.begin(); 
			it != typeDesc.m_fields.end(); ++it)
		{
			FieldDescriptor* fieldDesc = *it;
			fieldDesc->Write(csw);
		}
	}
	void SaveReflectionInfo(rde::Stream* stream)
	{
		rde::StreamWriter sw(stream);
		sw.WriteInt32(s_typeDescriptors.size());
		for (TypeMap::iterator it = s_typeDescriptors.begin(); it != s_typeDescriptors.end(); ++it)
		{
			const TypeDescPtr& desc = it->second;
			desc->WriteTypeInfo(sw);
		}
	}
	void SaveReflectionInfo(const char* fileName)
	{
		rde::FileStream fstream;
		if (fstream.Open(fileName, rde::iosys::AccessMode::WRITE))
			SaveReflectionInfo(&fstream);
	}

	void LoadTypes(rde::StreamReader& reader, rde::TypeRegistry& typeRegistry)
	{
		const long numTypes = reader.ReadInt32();
		char buffer[64];
		for (long i = 0; i < numTypes; ++i)
		{
			reader.ReadASCIIZ(buffer, sizeof(buffer));
			//if (typeRegistry.FindType(buffer) == 0)
			{
				const long typeSize = reader.ReadInt32();
				const long reflectionType = reader.ReadInt32();
				const long numElements = reader.ReadInt32();
				char baseClassName[64];
				reader.ReadASCIIZ(baseClassName, sizeof(baseClassName));
				const rde::uint16_t baseClassOffset = (rde::uint16_t)reader.ReadInt32();

				rde::Type* newType(0);
				if (reflectionType == rde::ReflectionType::POINTER)
				{
					newType = new rde::TypePointer(typeSize, buffer);
				}
				else if (reflectionType == rde::ReflectionType::ARRAY)
				{
					newType = new rde::TypeArray(typeSize, buffer, numElements);
				}
				else if (reflectionType == rde::ReflectionType::CLASS)
				{
					newType = new rde::TypeClass(typeSize, buffer, baseClassName, baseClassOffset);
				}
				if (typeRegistry.FindType(buffer) == 0)
					typeRegistry.AddType(newType);
			}
		}
	}
	void LoadFields(rde::StreamReader& reader, rde::TypeRegistry& typeRegistry)
	{
		const long numTypes = reader.ReadInt32();
		for (int i = 0; i < numTypes; ++i)
		{
			const long numFields = reader.ReadInt32();
			if (numFields > 0)
			{
				char typeName[64];
				reader.ReadASCIIZ(typeName, sizeof(typeName));
				rde::Type* type = typeRegistry.FindType(typeName);
				rde::TypeClass* typeClass = (rde::TypeClass*)type;
				for (int fieldIndex = 0; fieldIndex < numFields; ++fieldIndex)
				{
					char fieldTypeName[64];
					reader.ReadASCIIZ(fieldTypeName, sizeof(fieldTypeName));
					const int offset = reader.ReadInt32();
					char fieldName[64];
					reader.ReadASCIIZ(fieldName, sizeof(fieldName));
					rde::Field field;
					field.m_ownerClass = typeClass;
					field.m_offset = (rde::uint16_t)offset;
					field.m_name = fieldName;
					field.m_type = typeRegistry.FindType(fieldTypeName);
					typeClass->AddField(field);
				}
			}
		}
	}

	void LoadReflectionInfo(rde::Stream* stream, rde::TypeRegistry& typeRegistry)
	{
		rde::StreamReader sr(stream);
		rde::ChunkStreamReader csr(&sr);

		rde::ChunkStreamReader::ChunkHeader header;
		csr.BeginChunk(header);
		static const rde::StrId kTagTypes("Types");
		if (header.tag == kTagTypes.GetId())
		{
			LoadTypes(csr, typeRegistry);
		}
		csr.EndChunk(header);

		csr.BeginChunk(header);
		static const rde::StrId kTagFields("Fields");
		if (header.tag == kTagFields.GetId())
		{
			LoadFields(csr, typeRegistry);
		}
		csr.EndChunk(header);
	}
	void LoadReflectionInfo(const char* fileName, rde::TypeRegistry& typeRegistry)
	{
		rde::FileStream fstream;
		if (fstream.Open(fileName, rde::iosys::AccessMode::READ))
			LoadReflectionInfo(&fstream, typeRegistry);
	}

	typedef rde::fixed_vector<rde::StrId, 128, true> TypesToReflect;
	TypesToReflect	s_typesToReflect;

bool LoadTypesToReflect(const char* fileName)
{
	FILE* f = fopen(fileName, "rt");
	if (!f)
		return false;

	char lineBuffer[512];
	while (fgets(lineBuffer, sizeof(lineBuffer) - 1, f))
	{
		// Remove all whitespaces from the end.
		for (int i = 0; lineBuffer[i] != '\0'; ++i)
		{
			if (isspace(lineBuffer[i]))
			{
				lineBuffer[i] = '\0';
				break;
			}
		}
		s_typesToReflect.push_back(rde::StrId(lineBuffer));
	}

	fclose(f);
	return true;
}
bool ShouldBeReflected(const rde::StrId& symbolName)
{
	for (int i = 0; i < s_typesToReflect.size(); ++i)
	{
		const rde::StrId& typeId = s_typesToReflect[i];
		if (typeId == symbolName)
			return true;
	}
	return false;
}

} // <anonymous> namespace

bool LoadDataFromPDB(const char* pdbName,
					 IDiaDataSource** ppSource,
					 IDiaSession** ppSession,
					 IDiaSymbol** ppGlobal)
{
	wchar_t widePdbName[_MAX_PATH];
	mbstowcs_s(0, widePdbName, _MAX_PATH, pdbName, _MAX_PATH);

	HRESULT hr = ::CoInitialize(0);
	if (FAILED(hr))
	{
		printf("CoInitialize() failed.\n");
		return false;
	}
	hr = CoCreateInstance(__uuidof(DiaSource),
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        __uuidof(IDiaDataSource),
                        (void **) ppSource);
	if (FAILED(hr))
	{
		printf("CoCreateInstance() failed.\n");
		return false;
	}

	hr = (*ppSource)->loadDataFromPdb(widePdbName);
	if (FAILED(hr))
	{
		printf("loadDataFromPdb() failed [HR = 0x%X]\n", hr);
		return false;
	}

	hr = (*ppSource)->openSession(ppSession);
	if (FAILED(hr))
	{
		printf("openSession() failed [HR = 0x%X]\n", hr);
		return false;
	}

	hr = (*ppSession)->get_globalScope(ppGlobal);
	if (FAILED(hr))
	{
		printf("get_globalScope() failed [HR = 0x%X]\n", hr);
		return false;
	}
	return true;
}

void BStrToString(BSTR bstr, rde::StrId& outStr)
{
	const UINT bslen = SysStringLen(bstr);
	char* str = new char[bslen + 1];
	for (UINT i = 0; i < bslen; ++i)
	{
		str[i] = (char)((bstr[i] >= 32 && bstr[i] < 128) ? bstr[i] : '?');
	}
	str[bslen] = 0;
	outStr = str;
	delete[] str;
}
rde::StrId GetName(IDiaSymbol* symbol)
{
	rde::StrId retName;
	BSTR bstrName;
	if (symbol->get_name(&bstrName) != S_OK)
		return retName;
	BStrToString(bstrName, retName);
	SysFreeString(bstrName);
	return retName;
}

LONG GetSymbolOffset(IDiaSymbol* symbol)
{
	LONG offset(0);
	DWORD locType;
	if (symbol->get_locationType(&locType) == S_OK && locType == LocIsThisRel)
	{
		symbol->get_offset(&offset);
	}
	return offset;
}
size_t GetSymbolSize(IDiaSymbol* symbol)
{
	ULONGLONG len(0);
	symbol->get_length(&len);
	return (size_t)len;
}

const char* GetFundamentalTypeName(DWORD type, ULONGLONG typeSize)
{
	if (type == btUInt || type == btInt || type == btLong || type == btULong)
	{
		const bool typeSigned = (type == btInt || type == btLong);
		if (typeSize == 1)
			return typeSigned ? "int8" : "uint8";
		else if (typeSize == 2)
			return typeSigned ? "int16" : "uint16";
		else if (typeSize == 4)
			return typeSigned ? "int32" : "uint32";
		else if (typeSize == 8)
			return typeSigned ? "int64" : "uint64";
		else
			return "UnknownIntType";
	}
	else if (type == btBool)
	{
		return typeSize == 1 ? "bool" : "UnknownBoolType";
	}
	else if (type == btChar)
	{
		return typeSize == 1 ? "char" : "UnknownCharType";
	}
	else if (type == btFloat)
	{
		if (typeSize == 4)
			return "float";
		else if (typeSize == 8)
			return "double";
		else
			return "UnknownFloatType";
	}
	else
		return "UnknownFundamentalType";
}

// @TODO: We need to find a way to find where exactly symbol is defined
//        and extract field flags from comments.
//unsigned long GetFieldFlags(IDiaSymbol* symbol)
//{
//	BSTR bstrFileName;
//	if (symbol->get_sourceFileName(&bstrFileName) == S_OK)
//	{
//		std::string ret = BStrToString(bstrFileName);
//		printf("File: %s\n", ret.c_str());
//	}
//	return 0;
//}

TypeDescriptor* FindFieldType(IDiaSymbol* symbol)
{
	BOOL isStatic(FALSE);
	if (!SUCCEEDED(symbol->get_isStatic(&isStatic)) || isStatic)
		return 0;
	DWORD tag;
	if (symbol->get_symTag(&tag) != S_OK)
		return 0;

	TypeDescriptor* typeDesc(0);
	if (tag == SymTagArrayType)
	{
		IDiaSymbol* elementType(0);
		TypeDescriptor* elementTypeDesc(0);
		if (symbol->get_type(&elementType) == S_OK)
		{
			elementTypeDesc = FindFieldType(elementType);
			DWORD numElements(0);
			if (SUCCEEDED(symbol->get_count(&numElements)))
			{
				char nameSuffix[16];
				::sprintf_s(nameSuffix, "[%d]", numElements);
				rde::StrId tname(elementTypeDesc->m_name);
				tname.append(nameSuffix);
				typeDesc = AddTypeDescriptor(tname, rde::ReflectionType::ARRAY, GetSymbolSize(symbol));
				typeDesc->m_numElements = numElements;
			}
		}
	}
	else if (tag == SymTagPointerType)
	{
		IDiaSymbol* elementType(0);
		TypeDescriptor* elementTypeDesc(0);
		if (symbol->get_type(&elementType) == S_OK)
		{
			elementTypeDesc = FindFieldType(elementType);
			rde::StrId tname(elementTypeDesc->m_name);
			tname.append("*");
			typeDesc = AddTypeDescriptor(tname, rde::ReflectionType::POINTER, GetSymbolSize(symbol));
		}
	}
	else if (tag == SymTagUDT)
	{
		rde::StrId tname = GetName(symbol);
		typeDesc = AddTypeDescriptor(tname, rde::ReflectionType::CLASS, GetSymbolSize(symbol));
	}
	else if (tag == SymTagBaseType)
	{
		DWORD baseType;
		if (SUCCEEDED(symbol->get_baseType(&baseType)))
		{
			ULONGLONG typeSize;
			symbol->get_length(&typeSize);

			rde::StrId tname = GetFundamentalTypeName(baseType, typeSize);
			typeDesc = AddTypeDescriptor(tname, rde::ReflectionType::FUNDAMENTAL, typeSize);
		}
	}
	return typeDesc;
}

void ProcessSymbolChild(IDiaSymbol* symbol, const char* parentName)
{
	DWORD tag;
	if (symbol->get_symTag(&tag) != S_OK)
		return;

	if (tag == SymTagBaseClass)
	{
		rde::StrId baseName(GetName(symbol));
		LONG offset(0);
		symbol->get_offset(&offset);

		TypeDescriptor* desc = FindTypeDescriptor(parentName);
		desc->m_baseClassName = baseName;
		desc->m_baseClassOffset = (rde::uint16_t)offset;
	}
	else if (tag == SymTagData)
	{
		IDiaSymbol* typeSymbol(0);
		if (symbol->get_type(&typeSymbol) == S_OK)
		{
			TypeDescriptor* fieldTypeDesc = FindFieldType(typeSymbol);
			if (fieldTypeDesc)
			{
				fieldTypeDesc->m_size = GetSymbolSize(typeSymbol);

				FieldDescriptor* fieldDesc = new FieldDescriptor();
				fieldDesc->m_name = GetName(symbol);
				fieldDesc->m_typeName = fieldTypeDesc->m_name;
				fieldDesc->m_offset = GetSymbolOffset(symbol);

				TypeDescriptor* desc = FindTypeDescriptor(parentName);
				if (!desc->AddField(fieldDesc))
					delete fieldDesc;
			}
			typeSymbol->Release();
		}
	}
}
void ProcessTopLevelSymbol(IDiaSymbol* symbol)
{
	DWORD tag;
	if (symbol->get_symTag(&tag) != S_OK)
		return;

	if (tag == SymTagUDT)
	{
		rde::StrId name(GetName(symbol));
		if (ShouldBeReflected(name.GetStr()))
		{
			AddTypeDescriptor(name, rde::ReflectionType::CLASS, GetSymbolSize(symbol));

			IDiaEnumSymbols* enumChildren(0);
			if (SUCCEEDED(symbol->findChildren(SymTagNull, 0, nsNone, &enumChildren)))
			{
				IDiaSymbol* child(0);
				ULONG celt(0);
				while (SUCCEEDED(enumChildren->Next(1, &child, &celt)) && celt == 1)
				{
					ProcessSymbolChild(child, name.GetStr());

					child->Release();
				} // <for every child>

				enumChildren->Release();
			}
		} 
	}
}

void PrintFieldInfo(const rde::Field* f, void*)
{
	printf("\tField: %s, type: %s, offset: %d byte(s)\n", f->m_name.GetStr(), 
		(f->m_type ? f->m_type->m_name.GetStr() : "x"), f->m_offset);
}
void PrintTypeInfo(const rde::Type* t, void*)
{
	printf("Name: %s, size: %d\n", t->m_name.GetStr(), t->m_size);
	if (t->m_reflectionType == rde::ReflectionType::CLASS)
	{
		printf("Fields:\n");
		const rde::TypeClass* typeClass = (const rde::TypeClass*)t;
		typeClass->EnumerateFields(PrintFieldInfo, rde::ReflectionType::ALL, 0);
	}
}

void PrintHelp()
{
	printf("Usage:\n");
	printf("Reflector.exe file.pdb typesToReflect_file\n");
	printf("TypesToReflect_file should be plain text file with single type per line. Example:\n");
	printf("TypesToReflect.txt:\n");
	printf("Foo\n");
	printf("Bar\n");
}

int __cdecl main(int argc, char const *argv[])
{
	if (argc != 3)
	{
		PrintHelp();
		return 1;
	}
	const char* pdbFileName = argv[1];
	if (strstr(pdbFileName, ".pdb") == 0)
	{
		PrintHelp();
		return 1;
	}
	const char* typeListFileName = argv[2];
	if (!LoadTypesToReflect(typeListFileName))
	{
		printf("Unable to load list of files to reflect from file '%s'\n", typeListFileName);
		printf("This should be plain text file with single type per line. Example:\n");
		printf("TypesToReflect.txt:\n");
		printf("Foo\n");
		printf("Bar\n");
	}

	IDiaDataSource* dataSource(0);
	IDiaSession* session(0);
	IDiaSymbol* globalScope(0);
	if (!LoadDataFromPDB(pdbFileName, &dataSource, &session, &globalScope))
	{
		printf("Unable to load '%s'\n", pdbFileName);
		return 1;
	}

	IDiaEnumSymbols* enumSymbols(0);
	if (FAILED(globalScope->findChildren(SymTagUDT, NULL, nsNone, &enumSymbols)))
		return 1;

	rde::TypeRegistry typeRegistry;
#if LOAD_TEST
	LoadReflectionInfo("type.ref", typeRegistry);
#else
	IDiaSymbol* symbol(0);
	ULONG celt(0);
	while (SUCCEEDED(enumSymbols->Next(1, &symbol, &celt)) && celt == 1)
	{
		ProcessTopLevelSymbol(symbol);
		symbol->Release();
	}
	enumSymbols->Release();

	//RegisterClassTypes(typeRegistry);
#endif
	printf("Types:\n");
	typeRegistry.EnumerateTypes(PrintTypeInfo, 0);
#if !LOAD_TEST
	SaveReflectionInfo("type.ref");
#endif
	return 0;
}
