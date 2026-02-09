#ifndef JOIN_HH
#define JOIN_HH

#include "strCat.hh"

#include <iterator>
#include <string>
#include <utility>

namespace detail {

template<typename Collection, typename Separator>
class Joiner
{
public:
	Joiner(Collection&& col_, Separator&& sep_)
		: col(std::forward<Collection>(col_))
		, sep(std::forward<Separator>(sep_))
	{
	}

	[[nodiscard]] operator std::string() const
	{
		std::string result;
		auto first = std::begin(col);
		auto last  = std::end  (col);
		if (first != last) {
			result = strCat(*first);
			++first;
			while (first != last) {
				strAppend(result, sep, *first);
				++first;
			}
		}
		return result;
	}

private:
	Collection col;
	Separator sep;
};

} // namespace detail

// to enable strCat()
template<typename Col, typename Sep> struct strCatImpl::ConcatUnit<detail::Joiner<Col, Sep>>
	: public strCatImpl::ConcatViaString
{
	explicit ConcatUnit(const detail::Joiner<Col, Sep>& joiner)
		: strCatImpl::ConcatViaString(std::string(joiner)) {}
};


template<typename Collection, typename Separator>
[[nodiscard]] detail::Joiner<Collection, Separator> join(Collection&& col, Separator&& sep)
{
	return { std::forward<Collection>(col), std::forward<Separator>(sep) };
}

#endif
