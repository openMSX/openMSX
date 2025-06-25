#include "CassetteImage.hh"
#include "FileOperations.hh"
#include "Filename.hh"
#include <cassert>

namespace openmsx {

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

void CassetteImage::setFirstFileType(FileType type, const Filename& fileName) {
	if (type == UNKNOWN) {
		// see if there is a hint in the filename
		auto file = fileName.getResolved();
		auto fileStem = FileOperations::stem(file);

		auto containsInstructionAndCAS = [&](std::string_view instruction) {
			auto pos = fileStem.find(instruction);
			if (pos == std::string_view::npos) return false;
			return fileStem.find("CAS", pos + instruction.size()) != std::string_view::npos;
		};

		if (containsInstructionAndCAS("BLOAD")) {
			firstFileType = BINARY;
		} else if (fileStem.contains("CLOAD")) {
			// note: probably better to first check on CLOAD and then on RUN. Some filenames
			// contain hints like CLOAD + RUN.
			firstFileType = BASIC;
		} else if (containsInstructionAndCAS("RUN")) {
			firstFileType = ASCII;
		}
	} else {
		firstFileType = type;
	}
}

void CassetteImage::setSha1Sum(const Sha1Sum& sha1sum_)
{
	assert(sha1sum.empty());
	sha1sum = sha1sum_;
}

const Sha1Sum& CassetteImage::getSha1Sum() const
{
	assert(!sha1sum.empty());
	return sha1sum;
}

} // namespace openmsx
