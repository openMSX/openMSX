// $Id$

#ifndef __CASIMAGE_HH__
#define __CASIMAGE_HH__

#include <vector>
#include <string>
#include "CassetteImage.hh"
#include "openmsx.hh"

using std::vector;
using std::string;

class FileContext;


class CasImage : public CassetteImage
{
	public:
		CasImage(FileContext &context, const string &fileName);
		virtual ~CasImage();

		virtual short getSampleAt(const EmuTime &time);

	private:
		void writePulse(int f);
		void writeHeader(int s);
		void writeSilence(int s);
		void writeByte(byte b);
		bool writeData();
		void convert();
	
		unsigned pos, size;
		byte* buf;
		int baudRate;
		vector<char> output;
};

#endif
