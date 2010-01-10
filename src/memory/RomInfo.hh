// $Id$

#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include <string>
#include <set>

namespace openmsx {

class CliComm;

class RomInfo
{
public:
	RomInfo(const std::string& id,      const std::string& year,
	        const std::string& company, const std::string& country,
	        bool original,              const std::string& origType,
	        const std::string& remark,  const RomType& romType,
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

	static RomType nameToRomType(std::string name);
	static std::string romTypeToName(RomType type);
	static void getAllRomTypes(std::set<std::string>& result);

private:
	const std::string title;
	const std::string year;
	const std::string company;
	const std::string country;
	const std::string origType;
	const std::string remark;
	const RomType romType;
	const bool original;
	const int genMSXid;
};

} // namespace openmsx

#endif
