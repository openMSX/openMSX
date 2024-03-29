#ifndef MSXEXCEPTION_HH
#define MSXEXCEPTION_HH

#include "strCat.hh"

#include <concepts>
#include <string>

namespace openmsx {

class MSXException
{
public:
	explicit MSXException() = default;

	explicit MSXException(std::string message_)
		: message(std::move(message_)) {}

	template<typename T, typename... Args>
		requires(!std::same_as<MSXException, std::remove_cvref_t<T>>) // don't block copy-constructor
	explicit MSXException(T&& t, Args&&... args)
		: message(strCat(std::forward<T>(t), std::forward<Args>(args)...))
	{
	}

	[[nodiscard]] const std::string& getMessage() const &  { return message; }
	[[nodiscard]]       std::string  getMessage()       && { return std::move(message); }

private:
	std::string message;
};

class FatalError
{
public:
	explicit FatalError(std::string message_)
		: message(std::move(message_)) {}

	template<typename T, typename... Args>
		requires(!std::same_as<FatalError, std::remove_cvref_t<T>>) // don't block copy-constructor
	explicit FatalError(T&& t, Args&&... args)
		: message(strCat(std::forward<T>(t), std::forward<Args>(args)...))
	{
	}

	[[nodiscard]] const std::string& getMessage() const &  { return message; }
	[[nodiscard]]       std::string  getMessage()       && { return std::move(message); }

private:
	std::string message;
};

} // namespace openmsx

#endif
