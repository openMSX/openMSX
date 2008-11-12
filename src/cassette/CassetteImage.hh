// $Id$

#ifndef CASSETTEIMAGE_HH
#define CASSETTEIMAGE_HH

#include "EmuTime.hh"
#include <string>

namespace openmsx {

class CassetteImage
{
public:
	enum FileType { ASCII, BINARY, BASIC, UNKNOWN };

	virtual ~CassetteImage();
	virtual short getSampleAt(EmuTime::param time) = 0;
	virtual EmuTime getEndTime() const = 0;
	virtual unsigned getFrequency() const = 0;
	virtual void fillBuffer(unsigned pos, int** bufs, unsigned num) const = 0;

	FileType getFirstFileType() const;
	std::string getFirstFileTypeAsString() const;

protected:
	CassetteImage();
	void setFirstFileType(FileType type);

private:
	FileType firstFileType;
};

} // namespace openmsx

#endif
