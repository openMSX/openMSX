// $Id$

#include "FileManipulator.hh"
#include "CliComm.hh"
#include "CommandController.hh"
#include "StringOp.hh"
#include "DiskDrive.hh"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using std::set;
using std::string;
using std::transform;
using std::map;

namespace openmsx {


FileManipulator::FileManipulator()
{
	PRT_DEBUG("Trying to create a  FileManipulator");
	// register as command in the console
	std::string name("filemanipulator");
	CommandController::instance().registerCommand(this, name);
}

FileManipulator::~FileManipulator()
{
	PRT_DEBUG("Destroying FileManipulator object");
	// std::map<const int, byte*>::const_iterator it;
}

FileManipulator& FileManipulator::instance()
{
	static FileManipulator oneInstance;
	return oneInstance;

}

void FileManipulator::unregisterDrive(DiskDrive* drive, const std::string& ImageName)
{
	CliComm::instance().printInfo("unregistering drive with image " + ImageName);
	diskimages.erase(ImageName);
}

void FileManipulator::registerDrive(DiskDrive* drive, const std::string& ImageName)
{
	CliComm::instance().printInfo("registering drive with image " + ImageName);
	//CliComm::instance().printInfo( drive );
	// TODO: Below can probably be optimized and maybe we should throw a fatal error in such case
	for (std::map<const std::string, DiskDrive*>::const_iterator it = diskimages.begin(); it != diskimages.end(); ++it) {
		if ( ImageName == it->first ){
			CliComm::instance().printInfo("Reregistering drive with image " + ImageName + "? This shouldn't happen!");
		};
	}
	diskimages[ImageName]=drive;
}

std::string FileManipulator::help(const std::vector<std::string>& /*tokens*/) const
{
	return string("filemanipulator savedsk <drivename> <filename> : save drivename as dsk-file\nfilemanipulator extract <drivename> <dirname> : extract all files to <dir>\n");
}

void FileManipulator::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("savedsk");
		cmds.insert("import");
		cmds.insert("export");
		CommandController::completeString(tokens, cmds);
	} else if (tokens.size() == 3) {
		set<string> names;
		for (std::map<const std::string, DiskDrive*>::const_iterator it = diskimages.begin(); it != diskimages.end(); ++it) {
			names.insert( it->first );
		}
		CommandController::completeString(tokens, names);
	} else if (tokens.size() >= 4)
		if (tokens[1] == "savedsk" || tokens[1] == "import" || tokens[1] == "export") {
			CommandController::completeFileName(tokens);
		}
}

std::string FileManipulator::execute(const std::vector<std::string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		result += "Better names for this command are welcome\n";
	} else if (tokens[1] == "savedsk") {
		if ( tokens.size() != 4 ) {
			result += "incorrect number of parameters for savedsk\n";
		} else {
			result += "savedsk not implemented yet\n";
		}
		// CliComm::instance().update(CliComm::MEDIA, name, "");
	} else if (tokens[1] == "export") {
		if ( tokens.size() <= 3 ) {
			result += "incorrect number of parameters for export\n";
		} else {
			result += "export not implemented yet\n";
		}
	} else if (tokens[1] == "import") {
		if ( tokens.size() <= 3 ) {
			result += "incorrect number of parameters for import\n";
		} else {
			result += "import not implemented yet\n";
		}
	} else {
		result += std::string("unknown command " + tokens[1] + ", number of tokens :") + StringOp::toString(tokens.size()) + '\n';
	}
	return result;
}
} // namespace openmsx
