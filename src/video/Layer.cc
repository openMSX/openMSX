// $Id$

#include "Layer.hh"

namespace openmsx {

Layer::Layer(Coverage coverage_, ZIndex z_)
	: display(NULL), coverage(coverage_), z(z_)
{
}

Layer::~Layer()
{
}

void Layer::setCoverage(Coverage coverage_)
{
	coverage = coverage_;
}

void Layer::setZ(ZIndex z_)
{
	z = z_;
	if (display) display->updateZ(*this, z);
}

Layer::ZIndex Layer::getZ() const
{
	return z;
}

} // namespace openmsx

