// $Id$

#ifndef TYPEINFO_HH
#define TYPEINFO_HH

#include <typeinfo>
#include <cassert>

// Based on Loki::TypeInfo
//   offers a first-class, comparable wrapper over std::type_info
class TypeInfo
{
public:
	TypeInfo();
	TypeInfo(const std::type_info& ti);

	// Get the underlying std::type_info
	const std::type_info& get() const;

	// Compatibility functions
	bool before(const TypeInfo& rhs) const;
	const char* name() const;

private:
	const std::type_info* info;
};


inline TypeInfo::TypeInfo()
{
	class Dummy {};
	info = &typeid(Dummy);
	assert(info);
}

inline TypeInfo::TypeInfo(const std::type_info& ti)
	: info(&ti)
{
	assert(info);
}

inline const std::type_info& TypeInfo::get() const
{
	return *info;
}

inline bool TypeInfo::before(const TypeInfo& rhs) const
{
	// gcc version of std::type_info::before() returns bool
	return info->before(*rhs.info) != 0;
}

inline const char* TypeInfo::name() const
{
	return info->name();
}


// Comparison operators
inline bool operator==(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return lhs.get() == rhs.get();
}
inline bool operator<(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return lhs.before(rhs);
}
inline bool operator!=(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return !(lhs == rhs);
}
inline bool operator>(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return rhs < lhs;
}
inline bool operator<=(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return !(lhs > rhs);
}
inline bool operator>=(const TypeInfo& lhs, const TypeInfo& rhs)
{
	return !(lhs < rhs);
}

#endif
