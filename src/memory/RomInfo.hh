// $Id$

#ifndef __ROMINFO_HH__
#define __ROMINFO_HH__

#include <string>
#include <memory>
#include <set>
#include "RomTypes.hh"

namespace openmsx {

class Rom;
class XMLElement;

class RomInfo
{
public:
	RomInfo(const std::string& nid, const std::string& nyear,
		const std::string& ncompany, const std::string& nremark,
		const MapperType& nmapperType);
	~RomInfo();

	const std::string& getTitle()     const { return title; }
	const std::string& getYear()      const { return year; }
	const std::string& getCompany()   const { return company; }
	const std::string& getRemark()    const { return remark; }
	const MapperType& getMapperType() const { return mapperType; }
	void print();

	static std::auto_ptr<RomInfo> fetchRomInfo(const Rom& rom);
	static MapperType nameToMapperType(const std::string& name);
	static void getAllMapperTypes(std::set<std::string>& result);

private:
	/** Search for a ROM in the ROM database.
	  * @param rom ROM to look up.
	  * @return The information found in the database,
	  * 	or NULL if the given ROM is not in the database.
	  */
	static std::auto_ptr<RomInfo> searchRomDB(const Rom& rom);

	std::string title;
	std::string year;
	std::string company;
	std::string remark;
	MapperType mapperType;
};

} // namespace openmsx

#endif
