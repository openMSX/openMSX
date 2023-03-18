#ifndef IMGUI_PERSISTENT_HH
#define IMGUI_PERSISTENT_HH

#include "gl_vec.hh"
#include <cassert>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct ImGuiTextBuffer;

namespace openmsx {

using ImGuiPersistentType = std::variant<int, bool, std::string, float, gl::vec4>;

struct ImGuiPersistentItem
{
public:
	template<typename T>
	ImGuiPersistentItem(std::string name_, T currentValue_)
		: name(std::move(name_)), currentValue(currentValue_)
	{
	}

	template<typename T>
	[[nodiscard]] auto& get() {
		assert(std::holds_alternative<T>(currentValue));
		return std::get<T>(currentValue);
	}

public:
	std::string name;
	ImGuiPersistentType currentValue;
};

struct ImGuiPersistentSection
{
public:
	ImGuiPersistentSection(std::string sectionName_)
		: sectionName(std::move(sectionName_))
	{
	}

	[[nodiscard]] ImGuiPersistentItem* find(std::string_view name);
	[[nodiscard]] ImGuiPersistentItem& get(std::string_view name) {
		auto* item = find(name);
		assert(item);
		return *item;
	}
	[[nodiscard]] ImGuiPersistentItem& getOrCreate(std::string_view name) {
		if (auto* item = find(name)) {
			return *item;
		}
		auto* result = loadDynamic(*this, name, std::nullopt);
		assert(result);
		return *result;
	}

	void iniWrite(ImGuiTextBuffer* buf) const;
	void iniReadLine(const char* line);

public:
	std::string sectionName;
	std::deque<ImGuiPersistentItem> items; // need stable address on insert
	std::function<ImGuiPersistentItem*(ImGuiPersistentSection&, std::string_view, std::optional<std::string_view>)> loadDynamic;
};

class ImGuiManager
{
public:
	ImGuiManager(const ImGuiManager&) = delete;
	ImGuiManager& operator=(const ImGuiManager&) = delete;

	ImGuiManager();
	~ImGuiManager();

	[[nodiscard]] ImGuiPersistentSection* find(std::string_view name);
	[[nodiscard]] ImGuiPersistentSection& get(std::string_view name) {
		auto* item = find(name);
		assert(item);
		return *item;
	}

public:
	std::function<void(ImGuiManager&)> preSaveCallback;

private:
	// ini handler callbacks
	void* iniReadOpen(const char* name);
	void iniReadLine(void* entry, const char* line);
	void iniWriteAll(ImGuiTextBuffer* buf);

private:
	std::vector<ImGuiPersistentSection> sections;
};

} // namespace openmsx

#endif
