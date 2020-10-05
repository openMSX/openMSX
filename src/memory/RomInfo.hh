#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include "String32.hh"
#include <string_view>
#include <utility>
#include <vector>

namespace openmsx {

class RomInfo
{
public:
	RomInfo(String32 title_,   String32 year_,
	        String32 company_, String32 country_,
	        bool original_,    String32 origType_,
	        String32 remark_,  RomType romType_,
	        int genMSXid_)
		: title   (title_)
		, year    (year_)
		, company (company_)
		, country (country_)
		, origType(origType_)
		, remark  (std::move(remark_))
		, romType(romType_)
		, genMSXid(genMSXid_)
		, original(original_)
	{
	}

	std::string_view getTitle   (const char* buf) const {
		return fromString32(buf, title);
	}
	std::string_view getYear    (const char* buf) const {
		return fromString32(buf, year);
	}
	std::string_view getCompany (const char* buf) const {
		return fromString32(buf, company);
	}
	std::string_view getCountry (const char* buf) const {
		return fromString32(buf, country);
	}
	std::string_view getOrigType(const char* buf) const {
		return fromString32(buf, origType);
	}
	std::string_view getRemark  (const char* buf) const {
		return fromString32(buf, remark);
	}
	RomType          getRomType()   const { return romType; }
	bool             getOriginal()  const { return original; }
	int              getGenMSXid()  const { return genMSXid; }

	static RomType nameToRomType(std::string_view name);
	static std::string_view romTypeToName(RomType type);
	static std::vector<std::string_view> getAllRomTypes();
	static std::string_view getDescription(RomType type);
	static unsigned   getBlockSize  (RomType type);

private:
	String32 title;
	String32 year;
	String32 company;
	String32 country;
	String32 origType;
	String32 remark;
	RomType romType;
	int genMSXid;
	bool original;
};

} // namespace openmsx

#endif
