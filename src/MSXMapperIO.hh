// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "emutime.hh"
#include "MSXDevice.hh"

class MSXMapperIO : public MSXDevice
{
	public:
		~MSXMapperIO();
		static MSXMapperIO *instance();
		
		byte readIO(byte port, Emutime &time) = 0;
		void writeIO(byte port, byte value, Emutime &time);
		
		void init();
		void reset();
		
		//void saveState(std::ofstream &writestream);
		//void restoreState(std::string &devicestring, std::ifstream &readstream);

		// specific for MapperIO
		virtual void registerMapper(int blocks) = 0;
		byte getPageNum(int page);

	protected:
		MSXMapperIO();
	
	private:
		static MSXMapperIO *oneInstance;

		byte pageNum[4];
};

#endif //__MSXMAPPERIO_HH__

