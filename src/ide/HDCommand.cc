#include "HDCommand.hh"
#include "HD.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandException.hh"
#include "BooleanSetting.hh"
#include "TclObject.hh"

namespace openmsx {

using std::string;
using std::vector;

// class HDCommand

HDCommand::HDCommand(CommandController& commandController,
                     StateChangeDistributor& stateChangeDistributor,
                     Scheduler& scheduler, HD& hd_,
                     BooleanSetting& powerSetting_)
	: RecordedCommand(commandController, stateChangeDistributor,
	                  scheduler, hd_.getName())
	, hd(hd_)
	, powerSetting(powerSetting_)
{
}

void HDCommand::execute(array_ref<TclObject> tokens, TclObject& result,
                        EmuTime::param /*time*/)
{
	if (tokens.size() == 1) {
		result.addListElement(hd.getName() + ':');
		result.addListElement(hd.getImageName().getResolved());

		if (hd.isWriteProtected()) {
			TclObject options;
			options.addListElement("readonly");
			result.addListElement(options);
		}
	} else if ((tokens.size() == 2) ||
	           ((tokens.size() == 3) && tokens[1].getString() == "insert")) {
		if (powerSetting.getBoolean()) {
			throw CommandException(
				"Can only change hard disk image when MSX "
				"is powered down.");
		}
		int fileToken = 1;
		if (tokens[1].getString() == "insert") {
			if (tokens.size() > 2) {
				fileToken = 2;
			} else {
				throw CommandException(
					"Missing argument to insert subcommand");
			}
		}
		try {
			Filename filename(tokens[fileToken].getString().str(),
			                  UserFileContext());
			hd.switchImage(filename);
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
	vector<const char*> extra;
	if (tokens.size() < 3) {
		extra = { "insert" };
	}
	completeFileName(tokens, UserFileContext(), extra);
}

bool HDCommand::needRecord(const vector<string>& tokens) const
{
	return tokens.size() > 1;
}

} // namespace openmsx
