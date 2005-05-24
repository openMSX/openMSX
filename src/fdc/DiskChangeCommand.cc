// $Id$

#include "DiskChangeCommand.hh"
#include "CommandController.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandException.hh"

using std::string;
using std::vector;

namespace openmsx {

string DiskChangeCommand::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() == 1) {
		const string& diskName = getCurrentDiskName();
		if (!diskName.empty()) {
			result += "Current disk: " + diskName + '\n';
		} else {
			result += "There is currently no disk inserted in drive \""
			       + getDriveName() + "\"\n";
		}
	} else if (tokens[1] == "-ramdsk") {
		vector<string> nopatchfiles;
		insertDisk(tokens[1], nopatchfiles);
	} else if (tokens[1] == "-eject") {
		ejectDisk();
	} else if (tokens[1] == "eject") {
		ejectDisk();
		result += "Warning: use of 'eject' is deprecated, instead use '-eject'";
	} else {
		try {
			UserFileContext context;
			vector<string> patches;
			for (unsigned i = 2; i < tokens.size(); ++i) {
				patches.push_back(context.resolve(tokens[i]));
			}
			insertDisk(context.resolve(tokens[1]), patches);
		} catch (FileException &e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string DiskChangeCommand::help(const vector<string>& /*tokens*/) const
{
	const string& name = getDriveName();
	return name + " -eject      : remove disk from virtual drive\n" +
	       name + " -ramdsk     : create a virtual disk in RAM\n" +
	       name + " <filename> : change the disk file\n";
}

void DiskChangeCommand::tabCompletion(vector<string>& tokens) const
{
	// TODO insert "-eject", "-ramdsk"
	if (tokens.size() >= 2) {
		CommandController::completeFileName(tokens);
	}
}

} // namespace openmsx
