#include "DiskImageCLI.hh"

#include "CommandLineParser.hh"
#include "Interpreter.hh"
#include "MSXException.hh"
#include "TclObject.hh"

namespace openmsx {

std::span<const std::string_view> DiskImageCLI::getExtensions()
{
	static constexpr std::array<std::string_view, 7> extensions = {
		"di1", "di2", "dmk", "dsk", "xsa", "fd1", "fd2"
	};
	return extensions;
}

DiskImageCLI::DiskImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	for (const auto* disk : {"-diska", "-diskb"}) {
		parser.registerOption(disk, *this);
	}
	parser.registerFileType(getExtensions(), *this);
}

void DiskImageCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	std::string filename = getArgument(option, cmdLine);
	parse(zstring_view(option).substr(1), filename, cmdLine);
}
std::string_view DiskImageCLI::optionHelp() const
{
	return "Insert the disk image specified in argument";
}

void DiskImageCLI::parseFileType(const std::string& filename, std::span<std::string>& cmdLine)
{
	parse(tmpStrCat("disk", driveLetter), filename, cmdLine);
	++driveLetter;
}

std::string_view DiskImageCLI::fileTypeHelp() const
{
	return "Disk image";
}

std::string_view DiskImageCLI::fileTypeCategoryName() const
{
	return "disk";
}

void DiskImageCLI::parse(zstring_view drive, std::string_view image,
                         std::span<std::string>& cmdLine) const
{
	if (!parser.getInterpreter().hasCommand(drive)) {
		throw MSXException("No disk drive ", char(::toupper(drive.back())), " present to put image '", image, "' in.");
	}
	TclObject command = makeTclList(drive, image);
	while (peekArgument(cmdLine) == "-ips") {
		cmdLine = cmdLine.subspan(1);
		command.addListElement(getArgument("-ips", cmdLine));
	}
	command.executeCommand(parser.getInterpreter());
}

} // namespace openmsx
