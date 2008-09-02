// $Id$

#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"
#include "noncopyable.hh"

namespace openmsx {

class EmptyPatch : public PatchInterface, private noncopyable
{
public:
	EmptyPatch(const byte* block, unsigned size);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;
	virtual void getFilenames(std::vector<Filename>& result) const;

private:
	const byte* block;
	const unsigned size;
};

} // namespace openmsx

#endif
