// $Id$

#ifndef RESAMPLE_HH
#define RESAMPLE_HH

#include "Observer.hh"
#include "GlobalSettings.hh"
#include <memory>

namespace openmsx {

class ResampleAlgo;
class GlobalSettings;
class Setting;
template<typename T> class EnumSetting;

class Resample : private Observer<Setting>
{
public:
	/** Note: To enable various optimizations (like SSE), this method is
	  * allowed to generate up to 3 extra sample.
	  * @see SoundDevice::updateBuffer()
	  */
	virtual bool generateInput(int* buffer, unsigned num) = 0;

protected:
	Resample(GlobalSettings& globalSettings, unsigned channels);
	virtual ~Resample();
	void setResampleRatio(double inFreq, double outFreq);
	bool generateOutput(int* dataOut, unsigned num);

private:
	void createResampler();

	// Observer<Setting>
	void update(const Setting& setting);

	double ratio;
	std::auto_ptr<ResampleAlgo> algo;
	EnumSetting<GlobalSettings::ResampleType>& resampleSetting;
	const unsigned channels;
};

} // namespace openmsx

#endif
