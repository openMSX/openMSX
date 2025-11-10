#ifndef COMPLETION_CANDIDATE_HH
#define COMPLETION_CANDIDATE_HH

#include <string>

struct CompletionCandidate {
	std::string text;
	std::string display_ = {}; // empty means same as text
	bool partial = false;
	bool caseSensitive = true; // prefer to keep this 'true'

	[[nodiscard]] const std::string& display() const {
		return display_.empty() ? text : display_;
	}
};

#endif // COMPLETION_CANDIDATE_HH
