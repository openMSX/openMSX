// $Id$

#include "FileManipulator.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "File.hh"
#include "SectorBasedDisk.hh"
#include "DiskDrive.hh"
#include <cassert>

using std::set;
using std::string;
using std::vector;

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


void FileManipulator::registerDrive(RealDrive& drive, const string& imageName)
{
	assert(diskImages.find(imageName) == diskImages.end());
	diskImages[imageName] = &drive;
}

void FileManipulator::unregisterDrive(RealDrive& drive, const string& imageName)
{
	assert(diskImages.find(imageName) != diskImages.end());
	assert(diskImages[imageName] == &drive);
	diskImages.erase(imageName);
}


string FileManipulator::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		throw CommandException("Missing argument");
	} else if (tokens[1] == "savedsk") {
		if (tokens.size() != 4) {
			throw CommandException("Incorrect number of parameters");
		} else {
			DiskImages::const_iterator it = diskImages.find(tokens[2]);
			if (it != diskImages.end()) {
				savedsk(it->second, tokens[3]);
			}
		}
	} else if (tokens[1] == "export") {
		if (tokens.size() <= 3) {
			throw CommandException("Incorrect number of parameters");
		} else {
			throw CommandException("Not implemented yet");
		}
	} else if (tokens[1] == "import") {
		if (tokens.size() <= 3) {
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
	    "filemanipulator import <drivename> <dirname>   : import all files and subdirs from <dir>\n"
	    "filemanipulator export <drivename> <dirname>   : extract all files to <dir>\n"
	    "filemanipulator msxtree <drivename>            : show the dir+files structure\n"
	    "filemanipulator msxdir <drivename> <filename>  : long format dir of <filename>";
}

void FileManipulator::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> cmds;
		cmds.insert("savedsk");
		cmds.insert("import");
		cmds.insert("export");
		cmds.insert("msxtree");
		cmds.insert("msxdir");
		CommandController::completeString(tokens, cmds);
	} else if (tokens.size() == 3) {
		set<string> names;
		for (DiskImages::const_iterator it = diskImages.begin();
		     it != diskImages.end(); ++it) {
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

void FileManipulator::savedsk(RealDrive* drive, const string& filename)
{
	SectorBasedDisk* disk = dynamic_cast<SectorBasedDisk*>(&drive->getDisk());
	if (!disk) {
		// not a SectorBasedDisk
		throw CommandException("Unsupported disk type");
	}
	unsigned nrsectors = disk->getNbSectors();
	byte buf[SectorBasedDisk::SECTOR_SIZE];
	File file(filename);
	for (unsigned i = 0; i < nrsectors; ++i) {
		disk->readLogicalSector(i, buf);
		file.write(buf, SectorBasedDisk::SECTOR_SIZE);
	}
}

} // namespace openmsx
