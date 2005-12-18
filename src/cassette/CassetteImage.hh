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
	virtual ~CassetteImage() {}
	virtual short getSampleAt(const EmuTime& time) = 0;
	FileType getFirstFileType() { return firstFileType; }
	std::string getFirstFileTypeAsString() {
		if (firstFileType == ASCII) {
			return "ASCII";
		} else if (firstFileType == BINARY) {
			return "binary";
		} else if (firstFileType == BASIC) {
			return "BASIC";
		} else return "unknown";
	}
protected:
	void setFirstFileType(FileType type) { firstFileType = type; }
private:
	FileType firstFileType;
};

} // namespace openmsx

#endif
