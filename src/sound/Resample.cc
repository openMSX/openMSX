// $Id$

#include "Resample.hh"
#include "ResampleTrivial.hh"
#include "ResampleHQ.hh"
#include "ResampleLQ.hh"
#include "ResampleBlip.hh"
#include "EnumSetting.hh"
#include "unreachable.hh"
#include <cassert>

namespace openmsx {

Resample::Resample(EnumSetting<ResampleType>& resampleSetting_)
	: resampleSetting(resampleSetting_)
{
	resampleSetting.attach(*this);
}

Resample::~Resample()
{
	resampleSetting.detach(*this);
}

void Resample::setResampleRatio(double inFreq, double outFreq, bool stereo)
{
	channels = stereo ? 2 : 1;
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
				algo = ResampleLQ<1>::create(*this, ratio);
			} else {
				algo = ResampleLQ<2>::create(*this, ratio);
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
			UNREACHABLE;
		}
	}
}

} // namespace openmsx
