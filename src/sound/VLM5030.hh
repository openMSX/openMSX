// $Id$

#ifndef VLM5030_HH
#define VLM5030_HH

#include "SoundDevice.hh"
#include "ChannelMixer.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Rom;
class MSXMotherBoard;
class XMLElement;

class VLM5030 : public SoundDevice, private ChannelMixer, private Resample<1>
{
public:
	VLM5030(MSXMotherBoard& motherBoard, const std::string& name,
	        const std::string& desc, const XMLElement& config, 
	        const EmuTime& time);
        ~VLM5030();
        void reset(const EmuTime& time);

	// latch control data
	void writeData(byte data); // latch control data

	// get BSY pin level
	bool getBSY(const EmuTime& time);
	// set RST pin level : reset / set table address A8-A15
	void setRST(bool pin, const EmuTime& time);
	// set VCU pin level : ?? unknown
	void setVCU(bool pin, const EmuTime& time);
	// set ST pin level  : set table address A0-A7 / start speech 
	void setST(bool pin, const EmuTime& time);

private:
	// SoundDevice
	virtual void setVolume(int maxVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(
		unsigned length, int* buffer, const EmuTime& start,
		const EmuDuration& sampDur);

	// ChannelMixer
	virtual void generateChannels(int** bufs, unsigned num);

	// Resample
	virtual void generateInput(float* buffer, unsigned num);

	void setupParameter(byte param);
	int getBits(int sbit, int bits);
	int parseFrame();

	void checkMute();
	
	std::auto_ptr<Rom> rom;
	int address_mask;

	word address;
	bool pin_BSY;
	bool pin_ST;
	bool pin_VCU;
	bool pin_RST;
	byte latch_data;
	word vcu_addr_h;
	byte parameter;
	byte phase;

	// state of option paramter
	int frame_size;
	int pitch_offset;
	byte interp_step;

	byte interp_count; // number of interp periods
	byte sample_count; // sample number within interp
	byte pitch_count;

	// these contain data describing the current and previous voice frames
	word old_energy;
	byte old_pitch;
	signed_word old_k[10];
	word target_energy;
	byte target_pitch;
	signed_word target_k[10];

	word new_energy;
	byte new_pitch;
	signed_word new_k[10];

	// these are all used to contain the current state of the sound generation
	unsigned int current_energy;
	unsigned int current_pitch;
	int current_k[10];

	int x[10];

	int maxVolume;
};

} // namespace openmsx

#endif
