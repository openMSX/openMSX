// $Id$

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include <memory>
#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "CommandLineParser.hh"
#include "SoundDevice.hh"

namespace openmsx {

class CassetteImage;
class XMLElement;

class MSXCassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
	MSXCassettePlayerCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
	virtual void parseFileType(const std::string& filename);
	virtual const std::string& fileTypeHelp() const;
};


class CassettePlayer : public CassetteDevice, public SoundDevice,
                       private SimpleCommand
{
public:
	CassettePlayer();
	virtual ~CassettePlayer();

	void insertTape(const std::string& filename);
	void removeTape();

	// CassetteDevice
	virtual void setMotor(bool status, const EmuTime& time);
	virtual short readSample(const EmuTime& time);
	virtual void writeWave(short* buf, int length);
	virtual int getWriteSampleRate();

	// Pluggable + SoundDevice
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(int length, int* buffer);

private:
	void rewind();
	void updatePosition(const EmuTime& time);
	short getSample(const EmuTime& time);

	std::auto_ptr<CassetteImage> cassette;
	bool motor, forcePlay;
	EmuTime tapeTime;
	EmuTime prevTime;

	// Tape Command
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	// SoundDevice
	int volume;
	EmuDuration delta;
	EmuTime playTapeTime;
	XMLElement* playerElem;
};

} // namespace openmsx

#endif // __CASSETTEPLAYER_HH__
