// $Id$

#include <stdlib.h>
#include "CassettePlayer.hh"
#include "PluggingController.hh"
#include "CommandController.hh"
#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"
#include "File.hh"


MSXCassettePlayerCLI msxCassettePlayerCLI;

MSXCassettePlayerCLI::MSXCassettePlayerCLI()
{
	CommandLineParser::instance()->registerOption("-cassetteplayer", this);
	CommandLineParser::instance()->registerFileType("wav", this);
}

void MSXCassettePlayerCLI::parseOption(const std::string &option,
                            std::list<std::string> &cmdLine)
{
	std::string filename = cmdLine.front();
	cmdLine.pop_front();

	parseFileType(filename);
}
const std::string& MSXCassettePlayerCLI::optionHelp() const
{
	static const std::string text("Put raw tape image specified in argument in virtual\n\t\t\t  cassetteplayer");
	return text;
}

void MSXCassettePlayerCLI::parseFileType(const std::string &filename_)
{
	std::string filename(filename_); XML::Escape(filename);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << " <config id=\"cassetteplayer\">";
	s << "  <parameter name=\"filename\">" << filename << "</parameter>";
	s << " </config>";
	s << "</msxconfig>";

	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadStream(s);
}
const std::string& MSXCassettePlayerCLI::fileTypeHelp() const
{
	static const std::string text("Raw tape image, as recorded from real tape");
	return text;
}


CassettePlayer::CassettePlayer()
{
	motor = false;
	audioLength = 0;	// no tape inserted (yet)
	try {
		MSXConfig::Config *config =
			MSXConfig::Backend::instance()->getConfigById("cassetteplayer");
		std::string filename = config->getParameter("filename");
		insertTape(filename);
	} catch (MSXException& e) {
		PRT_DEBUG("Incorrect tape insertion!");
	}
	PluggingController::instance()->registerPluggable(this);
	CommandController::instance()->registerCommand(*this, "cassetteplayer");
}

CassettePlayer::~CassettePlayer()
{
	CommandController::instance()->unregisterCommand("cassetteplayer");
	PluggingController::instance()->unregisterPluggable(this);
	removeTape();	// free memory
}

void CassettePlayer::insertTape(const std::string &filename)
{
	// TODO throw exceptions instead of PRT_ERROR
	const char* file = File::findName(filename, TAPE).c_str();
	if (audioLength != 0)
		removeTape();
	if (SDL_LoadWAV(file, &audioSpec, &audioBuffer, &audioLength) == NULL)
		PRT_ERROR("CassettePlayer error: " << SDL_GetError());
	if (audioSpec.format != AUDIO_S16)
		PRT_ERROR("CassettePlayer error: unsupported WAV format");
	posReference = 0;	// rewind tape (make configurable??)
}

void CassettePlayer::removeTape()
{
	SDL_FreeWAV(audioBuffer);
	audioLength = 0;
}

int CassettePlayer::calcSamples(const EmuTime &time)
{
	if (audioLength) {
		// tape inserted
		float duration = (time - timeReference).toFloat();
		int samples = (int)(duration*audioSpec.freq);
		return posReference + samples;
	} else {
		// no tape
		return 0;
	}
}

void CassettePlayer::setMotor(bool status, const EmuTime &time)
{
	if (motor != status) {
		motor = status;
		if (motor) {
			// motor turned on
			PRT_DEBUG("CasPlayer motor on");
			timeReference = time;
		} else {
			// motor turned off
			PRT_DEBUG("CasPlayer motor off");
			posReference = calcSamples(time);
		}
	}
}

short CassettePlayer::readSample(const EmuTime &time)
{
	int samp;
	if (motor && audioLength) {
		// motor on and tape inserted
		Uint32 index = calcSamples(time);
		if (index < (audioLength/2)) {
			samp =  ((short*)audioBuffer)[index];
		} else {
			samp = 0;
		}
	} else {
		// motor off or no tape
		samp = 0;
	}
	//PRT_DEBUG("CasPlayer read "<<(int)samp);
	return samp;
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


const std::string &CassettePlayer::getName() const
{
	static const std::string name("cassetteplayer");
	return name;
}


void CassettePlayer::execute(const std::vector<std::string> &tokens,
                             const EmuTime &time)
{
	if (tokens.size() != 2)
		throw CommandException("Syntax error");
	if (tokens[1] == "eject") {
		print("Tape ejected");
		removeTape();
	} else {
		print("Changing tape");
		insertTape(tokens[1]);
	}
}

void CassettePlayer::help(const std::vector<std::string> &tokens) const
{
	print("tape eject      : remove tape from virtual player");
	print("tape <filename> : change the tape file");
}

void CassettePlayer::tabCompletion(std::vector<std::string> &tokens) const
{
	if (tokens.size() == 2)
		CommandController::completeFileName(tokens);
}
