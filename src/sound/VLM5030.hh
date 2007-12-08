// $Id$

#ifndef VLM5030_HH
#define VLM5030_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Rom;
class MSXMotherBoard;
class XMLElement;

class VLM5030 : public SoundDevice, private Resample
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
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(
		unsigned length, int* buffer, const EmuTime& start,
		const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	void setupParameter(byte param);
	int getBits(unsigned sbit, unsigned bits);
	int parseFrame();

	std::auto_ptr<Rom> rom;
	int address_mask;

	// state of option paramter
	int frame_size;
	int pitch_offset;

	// these contain data describing the current and previous voice frames
	// these are all used to contain the current state of the sound generation
	unsigned current_energy;
	unsigned current_pitch;
	int current_k[10];
	int x[10];

	word address;
	word vcu_addr_h;

	signed_word old_k[10];
	signed_word new_k[10];
	signed_word target_k[10];
	word old_energy;
	word new_energy;
	word target_energy;
	byte old_pitch;
	byte new_pitch;
	byte target_pitch;

	byte interp_step;
	byte interp_count; // number of interp periods
	byte sample_count; // sample number within interp
	byte pitch_count;

	byte latch_data;
	byte parameter;
	byte phase;
	bool pin_BSY;
	bool pin_ST;
	bool pin_VCU;
	bool pin_RST;
};

} // namespace openmsx

#endif
