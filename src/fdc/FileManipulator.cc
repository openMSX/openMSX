// $Id$

#include "FileManipulator.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include <cassert>

using std::set;
using std::string;
using std::vector;
using std::map;

namespace openmsx {

FileManipulator::FileManipulator()
{
	CommandController::instance().registerCommand(this, "filemanipulator");
}

FileManipulator::~FileManipulator()
{
	CommandController::instance().unregisterCommand(this, "filemanipulator");
}

FileManipulator& FileManipulator::instance()
{
	static FileManipulator oneInstance;
	return oneInstance;
}


void FileManipulator::registerDrive(DiskDrive& drive, const string& imageName)
{
	assert(diskimages.find(imageName) == diskimages.end());
	diskimages[imageName] = &drive;
}

void FileManipulator::unregisterDrive(DiskDrive& drive, const string& imageName)
{
	assert(diskimages.find(imageName) != diskimages.end());
	assert(diskimages[imageName] == &drive);
	diskimages.erase(imageName);
}


string FileManipulator::execute(const std::vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	} else if (tokens[1] == "savedsk") {
		if (tokens.size() != 4) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else if (tokens[1] == "export") {
		if ( tokens.size() <= 3 ) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else if (tokens[1] == "import") {
		if ( tokens.size() <= 3 ) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else {
		throw CommandException("Unknown subcommand: " + tokens[1]);
	}
	return result;
}

string FileManipulator::help(const vector<string>& /*tokens*/) const
{
	return
	    "filemanipulator savedsk <drivename> <filename> : save drivename as dsk-file\n"
	    "filemanipulator extract <drivename> <dirname>  : extract all files to <dir>";
}

void FileManipulator::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("savedsk");
		cmds.insert("import");
		cmds.insert("export");
		CommandController::completeString(tokens, cmds);
	} else if (tokens.size() == 3) {
		set<string> names;
		for (std::map<const string, DiskDrive*>::const_iterator it =
		         diskimages.begin(); it != diskimages.end(); ++it) {
		     names.insert(it->first);
		}
		CommandController::completeString(tokens, names);
	} else if (tokens.size() >= 4) {
		if ((tokens[1] == "savedsk") ||
		    (tokens[1] == "import")  ||
		    (tokens[1] == "export")) {
			CommandController::completeFileName(tokens);
		}
	}
}

} // namespace openmsx
