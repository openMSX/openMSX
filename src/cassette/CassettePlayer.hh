// $Id$

#ifndef __CASSETTEPLAYER_HH__
#define __CASSETTEPLAYER_HH__

#include <memory>
#include "CassetteDevice.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "CommandLineParser.hh"
#include "SoundDevice.hh"
#include "MSXException.hh"

using std::auto_ptr;

namespace openmsx {

class CassetteImage;

class MSXCassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
	MSXCassettePlayerCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const string& option,
			list<string>& cmdLine);
	virtual const string& optionHelp() const;
	virtual void parseFileType(const string& filename);
	virtual const string& fileTypeHelp() const;
};


class CassettePlayer : public CassetteDevice, public SoundDevice,
	private SimpleCommand
{
public:
	CassettePlayer();
	virtual ~CassettePlayer();

	void insertTape(const string& filename);
	void removeTape();

	// CassetteDevice
	virtual void setMotor(bool status, const EmuTime& time);
	virtual short readSample(const EmuTime& time);
	virtual void writeWave(short* buf, int length);
	virtual int getWriteSampleRate();

	// Pluggable + SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual int* updateBuffer(int length);

private:
	void rewind();
	void updatePosition(const EmuTime& time);
	short getSample(const EmuTime& time);

	auto_ptr<CassetteImage> cassette;
	bool motor, forcePlay;
	EmuTime tapeTime;
	EmuTime prevTime;

	// Tape Command
	virtual string execute(const vector<string>& tokens);
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;

	// SoundDevice
	int volume;
	EmuDuration delta;
	EmuTime playTapeTime;
};

} // namespace openmsx

#endif // __CASSETTEPLAYER_HH__
