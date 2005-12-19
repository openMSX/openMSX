// $Id$

#include "CassetteImage.hh"

namespace openmsx {

CassetteImage::CassetteImage()
	: firstFileType(UNKNOWN)
{
}

CassetteImage::~CassetteImage()
{
}

CassetteImage::FileType CassetteImage::getFirstFileType() const
{
	return firstFileType;
}

void CassetteImage::setFirstFileType(FileType type)
{
	firstFileType = type;
}

std::string CassetteImage::getFirstFileTypeAsString() const
{
	if (firstFileType == ASCII) {
		return "ASCII";
	} else if (firstFileType == BINARY) {
		return "binary";
	} else if (firstFileType == BASIC) {
		return "BASIC";
	} else {
		return "unknown";
	}
}

} // namespace openmsx
