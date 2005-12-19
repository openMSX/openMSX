// $Id$

#ifndef CASSETTEIMAGE_HH
#define CASSETTEIMAGE_HH

#include <string>

namespace openmsx {

class EmuTime;

class CassetteImage
{
public:
	enum FileType { ASCII, BINARY, BASIC, UNKNOWN };

	virtual ~CassetteImage();
	virtual short getSampleAt(const EmuTime& time) = 0;

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
