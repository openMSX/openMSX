// $Id$

#include <cassert>
#include "Scaler.hh"
#include "SimpleScaler.hh"
#include "SaI2xScaler.hh"
#include "Scale2xScaler.hh"
#include "HQ2xScaler.hh"
#include "openmsx.hh"

namespace openmsx {

// Force template instantiation.
template class Scaler<byte>;
template class Scaler<word>;
template class Scaler<unsigned int>;

template <class Pixel>
Scaler<Pixel>::Scaler()
{
}

template <class Pixel>
auto_ptr<Scaler<Pixel> > Scaler<Pixel>::createScaler(
	ScalerID id, SDL_PixelFormat* format)
{
	switch(id) {
	case SCALER_SIMPLE:
		return auto_ptr<Scaler<Pixel> >(new SimpleScaler<Pixel>());
	case SCALER_SAI2X:
		return auto_ptr<Scaler<Pixel> >(new SaI2xScaler<Pixel>(format));
	case SCALER_SCALE2X:
		return auto_ptr<Scaler<Pixel> >(new Scale2xScaler<Pixel>());
	case SCALER_HQ2X:
		return auto_ptr<Scaler<Pixel> >(sizeof(Pixel) == 1
			? new SimpleScaler<Pixel>()
			: new HQ2xScaler<Pixel>());
	default:
		assert(false);
		return auto_ptr<Scaler<Pixel> >();
	}
}

} // namespace openmsx

