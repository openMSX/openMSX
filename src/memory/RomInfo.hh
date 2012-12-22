// $Id$

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
	const std::string title;
	const std::string year;
	const std::string company;
	const std::string country;
	const std::string origType;
	const std::string remark;
	const RomType romType;
	const int genMSXid;
	const bool original;
};

} // namespace openmsx

#endif
