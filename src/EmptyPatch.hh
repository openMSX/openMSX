#ifndef EMPTYPATCH_HH
#define EMPTYPATCH_HH

#include "PatchInterface.hh"
#include "noncopyable.hh"

namespace openmsx {

class EmptyPatch : public PatchInterface, private noncopyable
{
public:
	EmptyPatch(const byte* block, size_t size);

	virtual void copyBlock(size_t src, byte* dst, size_t num) const;
	virtual size_t getSize() const;
	virtual std::vector<Filename> getFilenames() const;
	virtual bool isEmptyPatch() const { return true; }

private:
	const byte* block;
	const size_t size;
};

} // namespace openmsx

#endif
