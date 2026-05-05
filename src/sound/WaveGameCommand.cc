#include "WaveGameCommand.hh"

#include "WaveGame.hh"

#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "Filename.hh"
#include "TclObject.hh"

#include <array>
#include <string_view>

namespace openmsx {

using namespace std::literals;

WaveGameCommand::WaveGameCommand(
		WaveGame& waveGame_,
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "wavegame")
	, waveGame(waveGame_)
{
}

void WaveGameCommand::execute(
	std::span<const TclObject> tokens, TclObject& result, EmuTime /*time*/)
{
	if (tokens.size() == 1) {
		const auto& dir = waveGame.getSamplesDir();
		result = dir.empty() ? std::string_view{"empty"} : std::string_view{dir};

	} else if (tokens.size() == 2 && tokens[1] == "eject") {
		waveGame.eject();
		result = "Wave Game directory ejected";

	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		std::string dir(tokens[2].getString());
		try {
			dir = Filename(std::move(dir), userFileContext()).getResolved();
			if (!FileOperations::isDirectory(dir)) {
				throw CommandException(
					"Wave Game expects a directory, not a file: ", dir);
			}
			waveGame.setSamplesDir(dir);
			result = "Wave Game samples directory set to: ";
			result.addListElement(dir);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}

	} else {
		throw SyntaxError();
	}
}

std::string WaveGameCommand::help(std::span<const TclObject> /*tokens*/) const
{
	return "wavegame                : show current samples directory (or 'empty')\n"
	       "wavegame insert <path>  : point Wave Game at a directory of NN.wav / NN.cfg files\n"
	       "wavegame eject          : clear the samples directory\n";
}

void WaveGameCommand::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {"insert"sv, "eject"sv};
		completeFileName(tokens, userFileContext(), cmds);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		completeFileName(tokens, userFileContext());
	}
}

bool WaveGameCommand::needRecord(std::span<const TclObject> tokens) const
{
	// Only state-changing forms need to be recorded for replay.
	return tokens.size() > 1;
}

} // namespace openmsx
