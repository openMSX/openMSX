// $Id$

#include <stdlib.h>
#include "CassettePlayer.hh"
#include "PluggingController.hh"
#include "CommandController.hh"
#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"
#include "File.hh"
#include "CassetteImage.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "DummyCassetteImage.hh"
#include "Mixer.hh"
#include "RealTime.hh"


MSXCassettePlayerCLI msxCassettePlayerCLI;

MSXCassettePlayerCLI::MSXCassettePlayerCLI()
{
	CommandLineParser::instance()->registerOption("-cassetteplayer", this);
	CommandLineParser::instance()->registerFileType("rawtapeimages", this);
}

void MSXCassettePlayerCLI::parseOption(const string &option,
                                       list<string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
}
const string& MSXCassettePlayerCLI::optionHelp() const
{
	static const string text("Put raw tape image specified in argument in virtual cassetteplayer");
	return text;
}

void MSXCassettePlayerCLI::parseFileType(const string &filename_)
{
	string filename(filename_); XML::Escape(filename);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << " <config id=\"cassetteplayer\">";
	s << "  <parameter name=\"filename\">" << filename << "</parameter>";
	s << " </config>";
	s << "</msxconfig>";

	MSXConfig *config = MSXConfig::instance();
	config->loadStream(new UserFileContext(), s);
}
const string& MSXCassettePlayerCLI::fileTypeHelp() const
{
	static const string text("Raw tape image, as recorded from real tape");
	return text;
}


CassettePlayer::CassettePlayer()
	: cassette(NULL), motor(false)
{
	removeTape();
	
	MSXConfig *conf = MSXConfig::instance();
	if (conf->hasConfigWithId("cassetteplayer")) {
		Config *config = conf->getConfigById("cassetteplayer");
		const string &filename = config->getParameter("filename");
		try {
			insertTape(config->getContext(), filename);
		} catch (MSXException& e) {
			PRT_ERROR("Couldn't load tape image: " << filename);
		}
	} else {
		// no cassette image specified
	}
	PluggingController::instance()->registerPluggable(this);
	CommandController::instance()->registerCommand(this, "cassetteplayer");

	int bufSize = Mixer::instance()->registerSound("cassetteplayer", this,
	                                               5000, Mixer::MONO);
	buffer = new int[bufSize];
}

CassettePlayer::~CassettePlayer()
{
	Mixer::instance()->unregisterSound(this);
	delete[] buffer;
	
	CommandController::instance()->unregisterCommand(this, "cassetteplayer");
	PluggingController::instance()->unregisterPluggable(this);
	removeTape();	// free memory
}

void CassettePlayer::insertTape(FileContext *context,
                                const string &filename)
{
	CassetteImage *tmp;
	try {
		// first try WAV
		tmp = new WavImage(context, filename);
	} catch (MSXException &e) {
		// if that fails use CAS
		tmp = new CasImage(context, filename);
	}
	delete cassette;
	cassette = tmp;
	
	position = 0.0;	// rewind tape (make configurable??)
	playPos = 0.0;
}

void CassettePlayer::removeTape()
{
	delete cassette;
	cassette = new DummyCassetteImage();
}

float CassettePlayer::updatePosition(const EmuTime &time)
{
	float duration = (time - timeReference).toFloat();
	playPos = position + duration;
	return playPos;
}

void CassettePlayer::setMotor(bool status, const EmuTime &time)
{
	if (motor != status) {
		motor = status;
		if (motor) {
			// motor turned on
			//PRT_DEBUG("CassettePlayer motor on");
			timeReference = time;
		} else {
			// motor turned off
			//PRT_DEBUG("CassettePlayer motor off");
			position = updatePosition(time);
		}
	}
}

short CassettePlayer::getSample(float pos)
{
	if (motor) {
		return cassette->getSampleAt(pos);
	} else {
		return 0;
	}
}


short CassettePlayer::readSample(const EmuTime &time)
{
	return getSample(updatePosition(time));
}

void CassettePlayer::writeWave(short *buf, int length)
{
	// recording not implemented yet
}

int CassettePlayer::getWriteSampleRate()
{
	// recording not implemented yet
	return 0;	// 0 -> not interested in writeWave() data
}


const string &CassettePlayer::getName() const
{
	static const string name("cassetteplayer");
	return name;
}


void CassettePlayer::execute(const vector<string> &tokens)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	if (tokens[1] == "eject") {
		print("Tape ejected");
		removeTape();
	} else {
		try {
			print("Changing tape");
			UserFileContext context;
			insertTape(&context, tokens[1]);
		} catch (MSXException &e) {
			throw CommandException(e.getMessage());
		}
	}
}

void CassettePlayer::help(const vector<string> &tokens) const
{
	print("cassetteplayer eject      : remove tape from virtual player");
	print("cassetteplayer <filename> : change the tape file");
}

void CassettePlayer::tabCompletion(vector<string> &tokens) const
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}


void CassettePlayer::setInternalVolume(short newVolume)
{
	volume = newVolume;
}

void CassettePlayer::setSampleRate(int sampleRate)
{
	delta = 1.0 / sampleRate;
}

int* CassettePlayer::updateBuffer(int length)
{
	if (!motor) {
		return NULL;
	}
	int *buf = buffer;
	while (length--) {
		*(buf++) = (((int)getSample(playPos)) * volume) >> 15;
		playPos += delta;
	}
	return buffer;
}
