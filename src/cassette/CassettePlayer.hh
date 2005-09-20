// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "CommandLineParser.hh"
#include "SoundDevice.hh"
#include <memory>

namespace openmsx {

class CassetteImage;
class XMLElement;
class Mixer;
class CliComm;
class WavWriter;

class MSXCassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
    MSXCassettePlayerCLI(CommandLineParser& commandLineParser);
    virtual bool parseOption(const std::string& option,
							 std::list<std::string>& cmdLine);
    virtual const std::string& optionHelp() const;
    virtual void parseFileType(const std::string& filename,
	                           std::list<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	CommandLineParser& commandLineParser;
};


class CassettePlayer : public CassetteDevice, public SoundDevice,
                       private SimpleCommand
{
public:
	CassettePlayer(CliComm& cliComm, CommandController& commandController,
	               Mixer& mixer);
	virtual ~CassettePlayer();

	void insertTape(const std::string& filename, const EmuTime&);
	void removeTape(const EmuTime&);

	// CassetteDevice
	virtual void setMotor(bool status, const EmuTime& time);
	virtual short readSample(const EmuTime& time);
	virtual void setSignal(bool output, const EmuTime& time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(
		unsigned length, int* buffer, const EmuTime& time,
		const EmuDuration& sampDur);

private:
	void rewind(const EmuTime&);
	void updatePosition(const EmuTime& time);
	short getSample(const EmuTime& time);
	void fill_buf(size_t, int);
	void stopRecording(const EmuTime&);
	void setForce(bool status, const EmuTime& time);

	std::auto_ptr<CassetteImage> cassette;
	bool motor, forcePlay;
	EmuTime tapeTime;
	EmuTime recTime;
	EmuTime prevTime;

	uint32 samplesPerSecond;
	int last_sig, last_out;
	double part;
	bool _output;
	size_t sampcnt;
	unsigned char *buf;

	std::auto_ptr<WavWriter> wavWriter;

	// Tape Command
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	// SoundDevice
	int volume;
	EmuDuration delta;
	EmuTime playTapeTime;
	XMLElement* playerElem;

	CliComm& cliComm;
};

} // namespace openmsx

#endif
