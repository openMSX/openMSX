#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"

#include "String32.hh"
#include "stl.hh"
#include "view.hh"
#include "zstring_view.hh"

#include <string_view>

namespace openmsx {

class RomInfo
{
public:
	// This contains extra information for each RomType. This structure only
	// contains the primary (non-alias) rom types.
	struct RomTypeInfo {
		unsigned blockSize;
		zstring_view name;
		std::string_view description;
	};

public:
	RomInfo(String32 title_,   String32 year_,
	        String32 company_, String32 country_,
	        bool original_,    String32 origType_,
	        String32 remark_,  RomType romType_,
	        unsigned genMSXid_)
		: title   (title_)
		, year    (year_)
		, company (company_)
		, country (country_)
		, origType(origType_)
		, remark  (remark_)
		, romType (romType_)
		, genMSXid(genMSXid_)
		, original(original_)
	{
	}

	[[nodiscard]] std::string_view getTitle   (const char* buf) const {
		return fromString32(buf, title);
	}
	[[nodiscard]] std::string_view getYear    (const char* buf) const {
		return fromString32(buf, year);
	}
	[[nodiscard]] std::string_view getCompany (const char* buf) const {
		return fromString32(buf, company);
	}
	[[nodiscard]] std::string_view getCountry (const char* buf) const {
		return fromString32(buf, country);
	}
	[[nodiscard]] std::string_view getOrigType(const char* buf) const {
		return fromString32(buf, origType);
	}
	[[nodiscard]] std::string_view getRemark  (const char* buf) const {
		return fromString32(buf, remark);
	}
	[[nodiscard]] RomType          getRomType()   const { return romType; }
	[[nodiscard]] bool             getOriginal()  const { return original; }
	[[nodiscard]] unsigned         getGenMSXid()  const { return genMSXid; }

	[[nodiscard]] static RomType nameToRomType(std::string_view name);
	[[nodiscard]] static zstring_view     romTypeToName (RomType type);
	[[nodiscard]] static std::string_view getDescription(RomType type);
	[[nodiscard]] static unsigned         getBlockSize  (RomType type);
	[[nodiscard]] static auto getAllRomTypes() {
		return view::transform(getRomTypeInfo(), &RomTypeInfo::name);
	}
	[[nodiscard]] static const array_with_enum_index<RomType, RomInfo::RomTypeInfo>& getRomTypeInfo();

private:
	String32 title;
	String32 year;
	String32 company;
	String32 country;
	String32 origType;
	String32 remark;
	RomType romType;
	unsigned genMSXid;
	bool original;
};

} // namespace openmsx

#endif
