// $Id$

#ifndef __ROMINFO_HH__
#define __ROMINFO_HH__

#include "RomTypes.hh"
#include <string>
#include "EmuTime.hh"

class Rom;
class Device;

class RomInfo
{
	public:
		RomInfo(
			const std::string &nid, const std::string &nyear,
			const std::string &ncompany, const std::string &nremark,
			const MapperType &nmapperType
			);
		
		const std::string &getId() { return id; }
		const std::string &getYear() { return year; }
		const std::string &getCompany() { return company; }
		const std::string &getRemark() { return remark; }
		//const std::string &getRomType() { return romType; }
		const MapperType &getMapperType() const { return mapperType; }

		static RomInfo *fetchRomInfo(
			const Rom &rom, const Device &deviceConfig );
		static MapperType nameToMapperType(const std::string &name);
		void print();

	private:

		/** Search for a ROM in the ROM database.
		  * @param rom ROM to look up.
		  * @return The information found in the database,
		  * 	or NULL if the given ROM is not in the database.
		  */
		static RomInfo *searchRomDB(const Rom &rom);
		
		static MapperType guessMapperType(const byte* data, int size);
		
		std::string id;
		std::string year;
		std::string company;
		std::string remark;
		MapperType mapperType;
};


#endif
