// $Id$

#ifndef RESAMPLEDSOUNDDEVICE_HH
#define RESAMPLEDSOUNDDEVICE_HH

#include "SoundDevice.hh"
#include "Observer.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class ResampleAlgo;
class Setting;
template<typename T> class EnumSetting;

class ResampledSoundDevice : public SoundDevice, private Observer<Setting>
{
public:
	enum ResampleType { RESAMPLE_HQ, RESAMPLE_LQ, RESAMPLE_BLIP };

	/** Note: To enable various optimizations (like SSE), this method is
	  * allowed to generate up to 3 extra sample.
	  * @see SoundDevice::updateBuffer()
	  */
	bool generateInput(int* buffer, unsigned num);

protected:
	ResampledSoundDevice(MSXMotherBoard& motherBoard, const std::string& name,
	                     const std::string& description, unsigned channels,
	                     bool stereo = false);
	virtual ~ResampledSoundDevice();

	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual bool updateBuffer(unsigned length, int* buffer,
	                          EmuTime::param time);

	void createResampler();

private:
	// Observer<Setting>
	void update(const Setting& setting);

	EnumSetting<ResampleType>& resampleSetting;
	std::auto_ptr<ResampleAlgo> algo;
};

} // namespace openmsx

#endif
