#include "HDImageCLI.hh"
#include "CommandLineParser.hh"
#include "MSXException.hh"
#include "ranges.hh"
#include <utility>
#include <vector>

using std::string;

namespace openmsx {

namespace {
	struct IdImage {
		int id;
		string image;
	};
}
static std::vector<IdImage> images;

HDImageCLI::HDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-hda", *this, CommandLineParser::PHASE_BEFORE_MACHINE);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void HDImageCLI::parseOption(const string& option, span<string>& cmdLine)
{
	// Machine has not been loaded yet. Only remember the image.
	int id = option[3] - 'a';
	images.emplace_back(IdImage{id, getArgument(option, cmdLine)});
}

string HDImageCLI::getImageForId(int id)
{
	// HD queries image. Return (and clear) the remembered value, or return
	// an empty string.
	string result;
	if (auto it = ranges::find(images, id, &IdImage::id);
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
		char hd = char(::toupper('a' + images.front().id));
		throw MSXException("No hard disk ", hd, " present.");
	}
}

std::string_view HDImageCLI::optionHelp() const
{
	return "Use hard disk image in argument for the IDE or SCSI extensions";
}

} // namespace openmsx
