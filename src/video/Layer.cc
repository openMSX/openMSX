#include "Layer.hh"
#include "LayerListener.hh"

namespace openmsx {

Layer::Layer(Coverage coverage_, ZIndex z_)
	: display(nullptr), coverage(coverage_), z(z_)
{
}

Layer::~Layer()
{
}

void Layer::setZ(ZIndex z_)
{
	z = z_;
	if (display) display->updateZ(*this);
}


// class ScopedLayerHider

ScopedLayerHider::ScopedLayerHider(Layer& layer_)
	: layer(layer_)
	, originalCoverage(layer.getCoverage())
{
	layer.setCoverage(Layer::COVER_NONE);
}

ScopedLayerHider::~ScopedLayerHider()
{
	layer.setCoverage(originalCoverage);
}

} // namespace openmsx
