// $Id$

#include "HDCommand.hh"
#include "HD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "CliComm.hh"
#include "TclObject.hh"
#include <set>

namespace openmsx {

using std::string;
using std::vector;
using std::set;

// class HDCommand

HDCommand::HDCommand(CommandController& commandController,
                     MSXEventDistributor& msxEventDistributor,
                     Scheduler& scheduler, CliComm& cliComm_, HD& hd_)
	: RecordedCommand(commandController, msxEventDistributor,
	                  scheduler, hd_.getName())
	, hd(hd_)
	, cliComm(cliComm_)
{
}

void HDCommand::execute(const std::vector<TclObject*>& tokens, TclObject& result,
                        const EmuTime& /*time*/)
{
	if (tokens.size() == 1) {
		result.addListElement(hd.getName() + ':');
		result.addListElement(hd.file->getURL());
		// TODO: add write protected flag when this is implemented
		// result.addListElement("readonly");
	} else if ((tokens.size() == 2) ||
	           ((tokens.size() == 3) && tokens[1]->getString() == "insert")) {
		CommandController& controller = getCommandController();
		if (controller.getGlobalSettings().
				getPowerSetting().getValue()) {
			throw CommandException(
				"Can only change hard disk image when MSX "
				"is powered down.");
		}
		int fileToken = 1;
		if (tokens[1]->getString() == "insert") {
			if (tokens.size() > 2) {
				fileToken = 2;
			} else {
				throw CommandException(
					"Missing argument to insert subcommand");
			}
		}
		try {
			UserFileContext context;
			string filename = context.resolve(
				controller, tokens[fileToken]->getString());
			std::auto_ptr<File> newFile(new File(filename));
			hd.file = newFile;
			cliComm.update(CliComm::MEDIA, hd.getName(), filename);
			// Note: the diskX command doesn't do this either,
			// so this has not been converted to TclObject style here
			// return filename;
		} catch (FileException& e) {
			throw CommandException("Can't change hard disk image: " +
			                       e.getMessage());
		}
	} else {
		throw CommandException("Too many or wrong arguments.");
	}
}

string HDCommand::help(const vector<string>& /*tokens*/) const
{
	return hd.getName() + ": change the hard disk image for this hard disk drive\n";
}

void HDCommand::tabCompletion(vector<string>& tokens) const
{
	set<string> extra;
	if (tokens.size() < 3) {
		extra.insert("insert");
	}
	UserFileContext context;
	completeFileName(getCommandController(), tokens, context, extra);
}

} // namespace openmsx
