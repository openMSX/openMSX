// $Id$

#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

template <class Pixel>
Scaler<Pixel>::Scaler()
{
}

// Force template instantiation.
template class Scaler<byte>;
template class Scaler<word>;
template class Scaler<unsigned int>;

template <class Pixel>
Scaler<Pixel>* Scaler<Pixel>::createScaler(ScalerID id, Blender<Pixel> blender)
{
	switch(id) {
	case SCALER_SIMPLE:
		return new SimpleScaler<Pixel>();
	case SCALER_SAI2X:
		return new SaI2xScaler<Pixel>(blender);
	case SCALER_SCALE2X:
		return new Scale2xScaler<Pixel>();
	case SCALER_HQ2X:
		return sizeof(Pixel) == 1
			? new SimpleScaler<Pixel>()
			: new HQ2xScaler<Pixel>();
	default:
		assert(false);
		return NULL;
	}
}

} // namespace openmsx

