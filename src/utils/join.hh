#ifndef JOIN_HH
#define JOIN_HH

#include <iostream>
#include <iterator>
#include <sstream>
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

	friend std::ostream& operator<<(std::ostream& os, const Joiner& joiner)
	{
		return joiner.execute(os);
	}

	[[nodiscard]] operator std::string() const
	{
		std::ostringstream os;
		execute(os);
		return os.str();
	}

private:
	template<typename OutputStream>
	OutputStream& execute(OutputStream& os) const
	{
		auto first = std::begin(col);
		auto last  = std::end  (col);
		if (first != last) {
			goto print;
			while (first != last) {
				os << sep;
		print:	os << *first;
				++first;
			}
		}
		return os;
	}

private:
	Collection col;
	Separator sep;
};

} // namespace detail


template<typename Collection, typename Separator>
[[nodiscard]] detail::Joiner<Collection, Separator> join(Collection&& col, Separator&& sep)
{
	return { std::forward<Collection>(col), std::forward<Separator>(sep) };
}

#endif
