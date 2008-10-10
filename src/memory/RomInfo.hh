// $Id$

#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include "StringPool.hh"
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
	        const std::string& remark,  const RomType& romType);

	StringRef getTitle()     const { return title; }
	StringRef getYear()      const { return year; }
	StringRef getCompany()   const { return company; }
	StringRef getCountry()   const { return country; }
	StringRef getOrigType()  const { return origType; }
	StringRef getRemark()    const { return remark; }
	const RomType&     getRomType()   const { return romType; }
	bool               getOriginal()  const { return original; }

	void print(CliComm& cliComm) const;

	static RomType nameToRomType(std::string name);
	static std::string romTypeToName(RomType type);
	static void getAllRomTypes(std::set<std::string>& result);

private:
	const StringRef title;
	const StringRef year;
	const StringRef company;
	const StringRef country;
	const StringRef origType;
	const StringRef remark;
	const RomType romType;
	const bool original;
};

} // namespace openmsx

#endif
