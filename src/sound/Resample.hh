// $Id$

#ifndef RESAMPLE_HH
#define RESAMPLE_HH

#include "Observer.hh"
#include <memory>

namespace openmsx {

class ResampleAlgo;
class GlobalSettings;
class Setting;
template<typename T> class EnumSetting;

class Resample : private Observer<Setting>
{
public:
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
	const unsigned channels;
	EnumSetting<bool>& resampleSetting;
};

} // namespace openmsx

#endif
