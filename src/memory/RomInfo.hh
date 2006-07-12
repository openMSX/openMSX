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
	        const std::string& remark,  const RomType& romType);

	const std::string& getTitle()     const { return title; }
	const std::string& getYear()      const { return year; }
	const std::string& getCompany()   const { return company; }
	const std::string& getCountry()   const { return country; }
	const std::string& getRemark()    const { return remark; }
	const RomType& getRomType() const { return romType; }
	void print(CliComm& cliComm);

	static RomType nameToRomType(std::string name);
	static void getAllRomTypes(std::set<std::string>& result);

private:
	std::string title;
	std::string year;
	std::string company;
	std::string country;
	std::string remark;
	RomType romType;
};

} // namespace openmsx

#endif
