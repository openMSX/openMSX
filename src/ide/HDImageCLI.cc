#include "HDImageCLI.hh"

#include "CommandLineParser.hh"
#include "MSXException.hh"

#include <algorithm>
#include <utility>
#include <vector>

namespace openmsx {

namespace {
	struct IdImage {
		IdImage(int i, std::string m)
			: id(i), image(std::move(m)) {} // clang-15 workaround

		int id;
		std::string image;
	};
}
static std::vector<IdImage> images;

HDImageCLI::HDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-hda", *this, CommandLineParser::PHASE_BEFORE_MACHINE);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void HDImageCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	// Machine has not been loaded yet. Only remember the image.
	int id = option[3] - 'a';
	images.emplace_back(id, getArgument(option, cmdLine));
}

std::string HDImageCLI::getImageForId(int id)
{
	// HD queries image. Return (and clear) the remembered value, or return
	// an empty string.
	std::string result;
	if (auto it = std::ranges::find(images, id, &IdImage::id);
	    it != end(images)) {
		result = std::move(it->image);
		images.erase(it);
	}
	return result;
}

void HDImageCLI::parseDone()
{
	// After parsing all remembered values should be cleared. If not there
	// was no hard disk as specified.
	if (!images.empty()) {
		auto hd = char(::toupper('a' + images.front().id));
		throw MSXException("No hard disk ", hd, " present.");
	}
}

std::string_view HDImageCLI::optionHelp() const
{
	return "Use hard disk image in argument for the IDE or SCSI extensions";
}

} // namespace openmsx
