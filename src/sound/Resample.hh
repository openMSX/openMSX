// $Id$

#ifndef RESAMPLE_HH
#define RESAMPLE_HH

#include "Observer.hh"
#include <memory>

namespace openmsx {

class ResampleAlgo;
class Setting;
template<typename T> class EnumSetting;

class Resample : private Observer<Setting>
{
public:
	enum ResampleType { RESAMPLE_HQ, RESAMPLE_LQ, RESAMPLE_BLIP };

	/** Note: To enable various optimizations (like SSE), this method is
	  * allowed to generate up to 3 extra sample.
	  * @see SoundDevice::updateBuffer()
	  */
	virtual bool generateInput(int* buffer, unsigned num) = 0;

protected:
	Resample(EnumSetting<ResampleType>& resampleSetting, unsigned channels);
	virtual ~Resample();
	void setResampleRatio(double inFreq, double outFreq);
	bool generateOutput(int* dataOut, unsigned num);

private:
	void createResampler();

	// Observer<Setting>
	void update(const Setting& setting);

	double ratio;
	std::auto_ptr<ResampleAlgo> algo;
	EnumSetting<ResampleType>& resampleSetting;
	const unsigned channels;
};

} // namespace openmsx

#endif
