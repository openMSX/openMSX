// $Id$

#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class EmptyPatch : public PatchInterface
{
public:
	EmptyPatch(const byte* block, unsigned size);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;

private:
	const byte* block;
	unsigned size;
};

} // namespace openmsx

#endif
