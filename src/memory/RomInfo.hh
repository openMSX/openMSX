#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include "string_ref.hh"
#include <vector>

namespace openmsx {

class CliComm;

class RomInfo
{
public:
	RomInfo(string_ref id,      string_ref year,
	        string_ref company, string_ref country,
	        bool original,      string_ref origType,
	        string_ref remark,  const RomType& romType,
	        int genMSXid);

	RomInfo(RomInfo&& other)
		: title   (std::move(other.title))
		, year    (std::move(other.year))
		, company (std::move(other.company))
		, country (std::move(other.country))
		, origType(std::move(other.origType))
		, remark  (std::move(other.remark))
		, romType (std::move(other.romType))
		, genMSXid(std::move(other.genMSXid))
		, original(std::move(other.original))
	{
	}

	RomInfo& operator=(RomInfo&& other)
	{
		title    = std::move(other.title);
		year     = std::move(other.year);
		company  = std::move(other.company);
		country  = std::move(other.country);
		origType = std::move(other.origType);
		remark   = std::move(other.remark);
		romType  = std::move(other.romType);
		genMSXid = std::move(other.genMSXid);
		original = std::move(other.original);
		return *this;
	}

	const std::string& getTitle()     const { return title; }
	const std::string& getYear()      const { return year; }
	const std::string& getCompany()   const { return company; }
	const std::string& getCountry()   const { return country; }
	const std::string& getOrigType()  const { return origType; }
	const std::string& getRemark()    const { return remark; }
	const RomType&     getRomType()   const { return romType; }
	bool               getOriginal()  const { return original; }
	int                getGenMSXid()  const { return genMSXid; }

	static RomType nameToRomType(string_ref name);
	static string_ref romTypeToName(RomType type);
	static std::vector<std::string> getAllRomTypes();
	static string_ref getDescription(RomType type);
	static unsigned   getBlockSize  (RomType type);

private:
	std::string title;
	std::string year;
	std::string company;
	std::string country;
	std::string origType;
	std::string remark;
	RomType romType;
	int genMSXid;
	bool original;
};

} // namespace openmsx

#endif
