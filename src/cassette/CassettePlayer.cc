// $Id$

#include <cstdlib>
#include "CassettePlayer.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "xmlx.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CassetteImage.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "DummyCassetteImage.hh"
#include "Mixer.hh"
#include "RealTime.hh"
#include "CliCommOutput.hh"

namespace openmsx {

MSXCassettePlayerCLI::MSXCassettePlayerCLI(CommandLineParser& cmdLineParser)
{
	cmdLineParser.registerOption("-cassetteplayer", this);
	cmdLineParser.registerFileClass("rawtapeimage", this);
}

bool MSXCassettePlayerCLI::parseOption(const string &option,
                                       list<string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
	return true;
}
const string& MSXCassettePlayerCLI::optionHelp() const
{
	static const string text(
	  "Put raw tape image specified in argument in virtual cassetteplayer");
	return text;
}

void MSXCassettePlayerCLI::parseFileType(const string &filename)
{
	XMLElement& config = GlobalSettings::instance().getMediaConfig();
	XMLElement& playerElem = config.getCreateChild("cassetteplayer");
	playerElem.setData(filename);
	playerElem.setFileContext(auto_ptr<FileContext>(new UserFileContext()));
}
const string& MSXCassettePlayerCLI::fileTypeHelp() const
{
	static const string text("Raw tape image, as recorded from real tape");
	return text;
}


CassettePlayer::CassettePlayer()
	: motor(false), forcePlay(false)
{
	XMLElement& config = GlobalSettings::instance().getMediaConfig();
	playerElem = &config.getCreateChild("cassetteplayer");
	const string filename = playerElem->getData();
	if (!filename.empty()) {
		try {
			insertTape(playerElem->getFileContext().resolve(filename));
		} catch (MSXException& e) {
			throw FatalError("Couldn't load tape image: " + filename);
		}
	} else {
		removeTape();
	}

	static XMLElement cassettePlayerConfig("cassetteplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "5000")));
		cassettePlayerConfig.addChild(sound);
	}
	registerSound(cassettePlayerConfig);

	CommandController::instance().registerCommand(this, "cassetteplayer");
}

CassettePlayer::~CassettePlayer()
{
	CommandController::instance().unregisterCommand(this, "cassetteplayer");
	unregisterSound();
}

void CassettePlayer::insertTape(const string& filename)
{
	try {
		// first try WAV
		cassette.reset(new WavImage(filename));
	} catch (MSXException &e) {
		// if that fails use CAS
		cassette.reset(new CasImage(filename));
	}

	rewind();
	playerElem->setData(filename);
}

void CassettePlayer::removeTape()
{
	cassette.reset(new DummyCassetteImage());
	playerElem->setData("");
}

void CassettePlayer::rewind()
{
	tapeTime = EmuTime::zero;
	playTapeTime = EmuTime::zero;
}

void CassettePlayer::updatePosition(const EmuTime& time)
{
	if (motor || forcePlay) {
		tapeTime += (time - prevTime);
		playTapeTime = tapeTime;
	}
	prevTime = time;
}

void CassettePlayer::setMotor(bool status, const EmuTime& time)
{
	updatePosition(time);
	motor = status;
	setMute(!motor && !forcePlay);
}

short CassettePlayer::getSample(const EmuTime& time)
{
	if (motor || forcePlay) {
		return cassette->getSampleAt(time);
	} else {
		return 0;
	}
}

short CassettePlayer::readSample(const EmuTime& time)
{
	updatePosition(time);
	return getSample(tapeTime);
}

void CassettePlayer::writeWave(short* /*buf*/, int /*length*/)
{
	// recording not implemented yet
}

int CassettePlayer::getWriteSampleRate()
{
	// recording not implemented yet
	return 0;	// 0 -> not interested in writeWave() data
}


const string& CassettePlayer::getName() const
{
	static const string name("cassetteplayer");
	return name;
}

const string& CassettePlayer::getDescription() const
{
	static const string desc("Cassetteplayer, use to read .cas or .wav files.\n");
	return desc;
}

void CassettePlayer::plugHelper(Connector* /*connector*/, const EmuTime& /*time*/)
{
}

void CassettePlayer::unplugHelper(const EmuTime& /*time*/)
{
}


string CassettePlayer::execute(const vector<string>& tokens)
{
	string result;
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	if (tokens[1] == "eject") {
		result += "Tape ejected\n";
		removeTape();
		CliCommOutput::instance().update(CliCommOutput::MEDIA,
		                                 "cassetteplayer", "");
	} else if (tokens[1] == "rewind") {
		result += "Tape rewinded\n";
		rewind();
	} else if (tokens[1] == "force_play") {
		forcePlay = true;
		setMute(false);
	} else if (tokens[1] == "no_force_play") {
		forcePlay = false;
		setMute(!motor);
	} else {
		try {
			result += "Changing tape\n";
			UserFileContext context;
			insertTape(context.resolve(tokens[1]));
			CliCommOutput::instance().update(CliCommOutput::MEDIA,
			                         "cassetteplayer", tokens[1]);
		} catch (MSXException &e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string CassettePlayer::help(const vector<string>& /*tokens*/) const
{
	return "cassetteplayer eject         : remove tape from virtual player\n"
	       "cassetteplayer rewind        : rewind tape in virtual player\n"
	       "cassetteplayer force_play    : force playing of tape (no remote)\n"
	       "cassetteplayer no_force_play : don't force playing of tape\n"
	       "cassetteplayer <filename>    : change the tape file\n";
}

void CassettePlayer::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		CommandController::completeFileName(tokens);
	}
}


void CassettePlayer::setVolume(int newVolume)
{
	volume = newVolume;
}

void CassettePlayer::setSampleRate(int sampleRate)
{
	delta = EmuDuration(1.0 / sampleRate);
}

int *CassettePlayer::updateBuffer(int length)
{
	int *buf = buffer;
	while (length--) {
		*(buf++) = (((int)getSample(playTapeTime)) * volume) >> 15;
		playTapeTime += delta;
	}
	return buffer;
}

} // namespace openmsx
