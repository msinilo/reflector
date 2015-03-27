#ifndef CORE_CRC32_H
#define CORE_CRC32_H

#include "core/Config.h"

namespace rde
{
// Class representing CRC32 checksum value.
class CRC32
{
public:
	CRC32();
	explicit CRC32(const char* asciiz);

	void operator=(const char* asciiz);

	void Add8(uint8);
	void Add16(uint16);
	void Add32(uint32);

	void AddArray(const uint8* bytes, long numBytes);

	void Reset();
	uint32 GetValue() const	{ return m_value; }
	static uint32 GetValue(const char* asciiz);

private:
	uint32	m_value;
};

inline bool operator==(const CRC32& lhs, const CRC32& rhs)
{
	return lhs.GetValue() == rhs.GetValue();
}
inline bool operator!=(const CRC32& lhs, const CRC32& rhs)
{
	return !(lhs == rhs);
}
inline bool operator<(const CRC32& lhs, const CRC32& rhs)
{
	return lhs.GetValue() < rhs.GetValue();
}
}

#endif // CORE_CRC32_H
