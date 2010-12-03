// $Id$

#include "CassetteImage.hh"
#include <cassert>

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

void CassetteImage::setSha1Sum(const std::string& sha1sum_)
{
	assert(sha1sum.empty());
	sha1sum = sha1sum_;
}

const std::string& CassetteImage::getSha1Sum() const
{
	assert(!sha1sum.empty());
	return sha1sum;
}

} // namespace openmsx
