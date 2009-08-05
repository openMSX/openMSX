// $Id$

#include "Resample.hh"
#include "ResampleTrivial.hh"
#include "ResampleHQ.hh"
#include "ResampleLQ.hh"
#include "ResampleBlip.hh"
#include "EnumSetting.hh"
#include <cassert>

namespace openmsx {

Resample::Resample(EnumSetting<ResampleType>& resampleSetting_, unsigned channels_)
	: resampleSetting(resampleSetting_)
	, channels(channels_)
{
	assert((channels == 1) || (channels == 2));
	resampleSetting.attach(*this);
}

Resample::~Resample()
{
	resampleSetting.detach(*this);
}

void Resample::setResampleRatio(double inFreq, double outFreq)
{
	ratio = inFreq / outFreq;
	createResampler();
}

bool Resample::generateOutput(int* dataOut, unsigned num)
{
	return algo->generateOutput(dataOut, num);
}

void Resample::update(const Setting& setting)
{
	(void)setting;
	assert(&setting == &resampleSetting);
	createResampler();
}

void Resample::createResampler()
{
	if (ratio == 1.0) {
		algo.reset(new ResampleTrivial(*this));
	} else {
		switch (resampleSetting.getValue()) {
		case RESAMPLE_HQ:
			if (channels == 1) {
				algo.reset(new ResampleHQ<1>(*this, ratio));
			} else {
				algo.reset(new ResampleHQ<2>(*this, ratio));
			}
			break;
		case RESAMPLE_LQ:
			if (channels == 1) {
				algo.reset(new ResampleLQ<1>(*this, ratio));
			} else {
				algo.reset(new ResampleLQ<2>(*this, ratio));
			}
			break;
		case RESAMPLE_BLIP:
			if (channels == 1) {
				algo.reset(new ResampleBlip<1>(*this, ratio));
			} else {
				algo.reset(new ResampleBlip<2>(*this, ratio));
			}
			break;
		default:
			assert(false);
		}
	}
}

} // namespace openmsx
