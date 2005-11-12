// $Id$

#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include <string>
#include <memory>
#include <set>

namespace openmsx {

class Rom;
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

	static std::auto_ptr<RomInfo> fetchRomInfo(
		CliComm& cliComm, const Rom& rom);
	static RomType nameToRomType(std::string name);
	static void getAllRomTypes(std::set<std::string>& result);

private:
	/** Search for a ROM in the ROM database.
	  * @param rom ROM to look up.
	  * @return The information found in the database,
	  * 	or NULL if the given ROM is not in the database.
	  */
	static std::auto_ptr<RomInfo> searchRomDB(
		CliComm& cliComm, const Rom& rom);

	std::string title;
	std::string year;
	std::string company;
	std::string country;
	std::string remark;
	RomType romType;
};

} // namespace openmsx

#endif
