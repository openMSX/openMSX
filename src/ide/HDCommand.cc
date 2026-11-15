#include "HDCommand.hh"

#include "HD.hh"

#include "BooleanSetting.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "TclObject.hh"

#include "strCat.hh"

#include <array>

namespace openmsx {

// class HDCommand

HDCommand::HDCommand(CommandController& commandController_,
                     StateChangeDistributor& stateChangeDistributor_,
                     Scheduler& scheduler_, HD& hd_,
                     const BooleanSetting& powerSetting_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, hd_.getName())
	, hd(hd_)
	, powerSetting(powerSetting_)
{
}

void HDCommand::execute(std::span<const TclObject> tokens, TclObject& result,
                        EmuTime /*time*/)
{
	if (tokens.size() == 1) {
		result.addListElement(tmpStrCat(hd.getName(), ':'),
		                      hd.getImageName().getResolved());

		TclObject options;
		if (hd.isEjected())        options.addListElement("empty");
		if (hd.isWriteProtected()) options.addListElement("readonly");
		if (hd.isHotSwappable())   options.addListElement("hotswappable");
		if (options.size() != 0) result.addListElement(options);
	} else if ((tokens.size() == 2) && (tokens[1] == "eject")) {
		if (!hd.isHotSwappable()) {
			throw CommandException(
				"Can't eject a hard disk image, only an SD card.");
		}
		hd.eject();
	} else if ((tokens.size() == 2) ||
	           ((tokens.size() == 3) && tokens[1] == "insert")) {
		if (!hd.isHotSwappable() && powerSetting.getBoolean()) {
			throw CommandException(
				"Can only change hard disk image when MSX "
				"is powered down.");
		}
		int fileToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				fileToken = 2;
			} else {
				throw CommandException(
					"Missing argument to insert subcommand");
			}
		}
		try {
			Filename filename(tokens[fileToken].getString(),
			                  userFileContext());
			hd.switchImage(filename);
			// Note: the diskX command doesn't do this either,
			// so this has not been converted to TclObject style here
			// return filename;
		} catch (FileException& e) {
			throw CommandException("Can't change hard disk image: ",
			                       e.getMessage());
		}
	} else {
		throw CommandException("Too many or wrong arguments.");
	}
}

std::string HDCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return strCat(hd.getName(), ": change the hard disk image for this hard disk drive\n",
	              hd.isHotSwappable()
	                  ? "This is an SD card: the image can be changed while the MSX is powered on,\n"
	                    "and 'eject' removes the card.\n"
	                  : "");
}

void HDCommand::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array extra = {"eject"sv, "insert"sv};
	// 'eject' only for SD cards
	auto subCommands = std::span(extra).subspan(hd.isHotSwappable() ? 0 : 1);
	completeFileName(tokens, userFileContext(),
		(tokens.size() < 3) ? subCommands : std::span<const std::string_view>{});
}

bool HDCommand::needRecord(std::span<const TclObject> tokens) const
{
	return tokens.size() > 1;
}

} // namespace openmsx
