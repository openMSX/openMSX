#include "CassettePlayerCommand.hh"
#include "CassettePlayer.hh"
#include "CommandController.hh"
#include "StateChangeDistributor.hh"
#include "Scheduler.hh"

using std::string;

namespace openmsx {

CassettePlayerCommand::CassettePlayerCommand(
		CassettePlayer* cassettePlayer_,
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "cassetteplayer")
	, cassettePlayer(cassettePlayer_)
{
}

void CassettePlayerCommand::execute(
	std::span<const TclObject> tokens, TclObject& result, EmuTime::param time)
{
	if (!cassettePlayer) {
		throw CommandException("There is no cassetteplayer available.");
	}

	auto stopRecording = [&] {
		if (cassettePlayer->getState() == CassettePlayer::State::RECORD) {
			try {
				cassettePlayer->playTape(cassettePlayer->getImageName(), time);
				return true;
			} catch (MSXException& e) {
				throw CommandException(std::move(e).getMessage());
			}
		}
		return false; // was not recording
	};

	if (tokens.size() == 1) {
		// Returning Tcl lists here, similar to the disk commands in
		// DiskChanger
		TclObject options = makeTclList(cassettePlayer->getStateString());
		result.addListElement(tmpStrCat(getName(), ':'),
		                      cassettePlayer->getImageName().getResolved(),
		                      options);

	} else if (tokens[1] == "new") {
		std::string_view prefix = "openmsx";
		string filename = FileOperations::parseCommandFileArgument(
			(tokens.size() == 3) ? tokens[2].getString() : string{},
			CassettePlayer::TAPE_RECORDING_DIR, prefix, CassettePlayer::TAPE_RECORDING_EXTENSION);
		cassettePlayer->recordTape(Filename(filename), time);
		result = tmpStrCat(
			"Created new cassette image file: ", filename,
			", inserted it and set recording mode.");

	} else if (tokens[1] == "insert" && tokens.size() == 3) {
		try {
			result = "Changing tape";
			Filename filename(tokens[2].getString(), userFileContext());
			cassettePlayer->playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}

	} else if (tokens[1] == "motorcontrol" && tokens.size() == 3) {
		if (tokens[2] == "on") {
			cassettePlayer->setMotorControl(true, time);
			result = "Motor control enabled.";
		} else if (tokens[2] == "off") {
			cassettePlayer->setMotorControl(false, time);
			result = "Motor control disabled.";
		} else {
			throw SyntaxError();
		}

	} else if (tokens[1] == "setpos" && tokens.size() == 3) {
		stopRecording();
		cassettePlayer->setTapePos(time, tokens[2].getDouble(getInterpreter()));

	} else if (tokens.size() != 2) {
		throw SyntaxError();

	} else if (tokens[1] == "motorcontrol") {
		result = tmpStrCat("Motor control is ",
		                (cassettePlayer->motorControl ? "on" : "off"));

	} else if (tokens[1] == "record") {
			result = "TODO: implement this... (sorry)";

	} else if (tokens[1] == "play") {
		if (stopRecording()) {
			result = "Play mode set, rewinding tape.";
		} else if (cassettePlayer->getState() == CassettePlayer::State::STOP) {
			throw CommandException("No tape inserted or tape at end!");
		} else {
			// PLAY mode
			result = "Already in play mode.";
		}

	} else if (tokens[1] == "eject") {
		result = "Tape ejected";
		cassettePlayer->removeTape(time);

	} else if (tokens[1] == "rewind") {
		string r = stopRecording() ? "First stopping recording... " : "";
		cassettePlayer->rewind(time);
		r += "Tape rewound";
		result = r;

	} else if (tokens[1] == "getpos") {
		result = cassettePlayer->getTapePos(time);

	} else if (tokens[1] == "getlength") {
		result = cassettePlayer->getTapeLength(time);

	} else {
		try {
			result = "Changing tape";
			Filename filename(tokens[1].getString(), userFileContext());
			cassettePlayer->playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}
	}
	//if (!cassettePlayer->getConnector()) {
	//	cassettePlayer->cliComm.printWarning("Cassette player not plugged in.");
	//}
}

string CassettePlayerCommand::help(std::span<const TclObject> tokens) const
{
	string helpText;
	if (tokens.size() >= 2) {
		if (tokens[1] == "eject") {
			helpText =
			    "Well, just eject the cassette from the cassette "
			    "player/recorder!";
		} else if (tokens[1] == "rewind") {
			helpText =
			    "Indeed, rewind the tape that is currently in the "
			    "cassette player/recorder...";
		} else if (tokens[1] == "motorcontrol") {
			helpText =
			    "Setting this to 'off' is equivalent to "
			    "disconnecting the black remote plug from the "
			    "cassette player: it makes the cassette player "
			    "run (if in play mode); the motor signal from the "
			    "MSX will be ignored. Normally this is set to "
			    "'on': the cassetteplayer obeys the motor control "
			    "signal from the MSX.";
		} else if (tokens[1] == "play") {
			helpText =
			    "Go to play mode. Only useful if you were in "
			    "record mode (which is currently the only other "
			    "mode available).";
		} else if (tokens[1] == "new") {
			helpText =
			    "Create a new cassette image. If the file name is "
			    "omitted, one will be generated in the default "
			    "directory for tape recordings. Implies going to "
			    "record mode (why else do you want a new cassette "
			    "image?).";
		} else if (tokens[1] == "insert") {
			helpText =
			    "Inserts the specified cassette image into the "
			    "cassette player, rewinds it and switches to play "
			    "mode.";
		} else if (tokens[1] == "record") {
			helpText =
			    "Go to record mode. NOT IMPLEMENTED YET. Will be "
			    "used to be able to resume recording to an "
			    "existing cassette image, previously inserted with "
			    "the insert command.";
		} else if (tokens[1] == "getpos") {
			helpText =
			    "Return the position of the tape, in seconds from "
			    "the beginning of the tape.";
		} else if (tokens[1] == "setpos") {
			helpText =
			    "Wind the tape to the given position, in seconds from "
			    "the beginning of the tape.";
		} else if (tokens[1] == "getlength") {
			helpText =
			    "Return the length of the tape in seconds.";
		}
	} else {
		helpText =
		    "cassetteplayer eject             "
		    ": remove tape from virtual player\n"
		    "cassetteplayer rewind            "
		    ": rewind tape in virtual player\n"
		    "cassetteplayer motorcontrol      "
		    ": enables or disables motor control (remote)\n"
		    "cassetteplayer play              "
		    ": change to play mode (default)\n"
		    "cassetteplayer record            "
		    ": change to record mode (NOT IMPLEMENTED YET)\n"
		    "cassetteplayer new [<filename>]  "
		    ": create and insert new tape image file and go to record mode\n"
		    "cassetteplayer insert <filename> "
		    ": insert (a different) tape file\n"
		    "cassetteplayer getpos            "
		    ": query the position of the tape\n"
		    "cassetteplayer setpos <new-pos>  "
		    ": wind the tape to the given position\n"
		    "cassetteplayer getlength         "
		    ": query the total length of the tape\n"
		    "cassetteplayer <filename>        "
		    ": insert (a different) tape file\n";
	}
	return helpText;
}

void CassettePlayerCommand::tabCompletion(std::vector<string>& tokens) const
{
	using namespace std::literals;
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {
			"eject"sv, "rewind"sv, "motorcontrol"sv, "insert"sv, "new"sv,
			"play"sv, "getpos"sv, "setpos"sv, "getlength"sv,
			//"record"sv,
		};
		completeFileName(tokens, userFileContext(), cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "insert")) {
		completeFileName(tokens, userFileContext());
	} else if ((tokens.size() == 3) && (tokens[1] == "motorcontrol")) {
		static constexpr std::array extra = {"on"sv, "off"sv};
		completeString(tokens, extra);
	}
}

bool CassettePlayerCommand::needRecord(std::span<const TclObject> tokens) const
{
	return tokens.size() > 1;
}

} // namespace openmsx
