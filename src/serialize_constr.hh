#ifndef SERIALIZE_CONSTR_HH
#define SERIALIZE_CONSTR_HH

#include "serialize_meta.hh"
#include <tuple>

namespace openmsx {

/** Serialize (local) constructor arguments.
 *
 * Some classes don't have a default constructor. To be able to create new
 * instances of such classes, we need to invoke the constructor with some
 * parameters (see also Creator utility class above).
 *
 * SerializeConstructorArgs can be specialized for classes that need such extra
 * constructor arguments.
 *
 * This only stores the 'local' constructor arguments, this means the arguments
 * that are specific per instance. There can also be 'global' args, these are
 * known in the context where the class is being loaded (so these don't need to
 * be stored in the archive). See below for more details on global constr args.
 *
 * The serialize_as_enum class has the following members:
 *   using type = tuple<...>
 *     Tuple that holds the result of load() (see below)
 *   void save(Archive& ar, const T& t)
 *     This method should store the constructor args in the given archive
 *   type load(Archive& ar, unsigned version)
 *     This method should load the args from the given archive and return
 *     them in a tuple.
 */
template<typename T> struct SerializeConstructorArgs
{
	using type = std::tuple<>;
	void save(Archive auto& /*ar*/, const T& /*t*/) { }
	type load(Archive auto& /*ar*/, unsigned /*version*/) { return std::tuple<>(); }
};

} // namespace openmsx

#endif
