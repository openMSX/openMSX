#ifndef FOREACH_FILE_HH
#define FOREACH_FILE_HH

#include "FileOperations.hh"
#include "ReadDir.hh"
#include "StringOp.hh"
#include "one_of.hh"
#include <functional>
#include <string_view>
#include <tuple>
#include <type_traits>

/** This file implemented 3 utility functions:
 *  - foreach_file              (path, fileAction)
 *  - foreach_file_and_directory(path, fileAction, dirAction)
 *  - foreach_file_recursive    (path, fileAction)
 *
 * These traverse all files/directories in a directory and invoke some action
 * for respectively:
 *  - All files in this directory.
 *  - All files and all sub-directory in this directory (different action for both).
 *  - All files in the directory and recursively all sub-directories.
 *
 * The action(s) must accept the following parameters:
 *  - a '(const) std::string&' parameter:
 *     This is the full path for the current directory entry. Thus the initial
 *     path passed to foreach_xxx() appended with the name of the current
 *     entry. If this parameter is non-const, the action is allowed to modify
 *     it, BUT it MUST restore it to its original value before the action
 *     returns (e.g. you may temporarily extend this string).
 *  - Optionally: a 'std::string_view' parameter:
 *     This is the name of the current entry. It's a view on the last
 *     path-component from the previous parameter. If an action is not
 *     interested in this parameter it can omit it from its function signature.
 *  - Optionally: a 'FileOperations::Stat' parameter:
 *     This is the result of calling 'FileOperations::getStat()' on the current
 *     directory entry. If an action is not interested in this parameter it can
 *     omit it from its function signature. On some systems foreach_xxx() can
 *     be a bit more efficient when this information is not needed.
 *
 * The actions' return type must either be:
 *  - void
 *  - bool: when an action returns 'false', directory traversal will be aborted.
 *
 * The result of the foreach_xxx() function is
 *  - true: directory was fully traversed.
 *  - false: some action requested an abort.
 */

namespace openmsx {
	namespace detail {
		using namespace FileOperations;

		// Take an action that accepts the following parameters:
		// - a 'std::string&' or 'const std::string&' parameter
		// - optionally a std::string_view parameter
		// - optionally a 'FileOperations::Stat&' parameter
		// And returns a wrapper that makes the 2nd and 3rd parameter
		// non-optional, but (possibly) ignores those parameters when
		// delegating to the wrapped action.
		template<typename Action>
		[[nodiscard]] auto adaptParams(Action action) {
			if constexpr (std::is_invocable_v<Action, std::string&, std::string_view, Stat&>) {
				return std::tuple(action, true);
			} else if constexpr (std::is_invocable_v<Action, std::string&, std::string_view>) {
				return std::tuple([action](std::string& p, std::string_view f, Stat& /*st*/) {
				                          return action(p, f);
				                  }, false);
			} else if constexpr (std::is_invocable_v<Action, std::string&, Stat&>) {
				return std::tuple([action](std::string& p, std::string_view /*f*/, Stat& st) {
				                          return action(p, st);
				                  }, true);
			} else if constexpr (std::is_invocable_v<Action, std::string&>) {
				return std::tuple([action](std::string& p, std::string_view /*f*/, Stat& /*st*/) {
				                          return action(p);
				                  }, false);
			} else {
				static_assert((Action{}, false), "Wrong signature for action");
			}
		}

		// Take an action and
		// - return it unchanged if it already returned a non-void result
		// - otherwise return a wrapper that invokes the given action and then returns 'true'.
		template<typename Action>
		[[nodiscard]] auto adaptReturn(Action action) {
			using ResultType = std::invoke_result_t<Action, std::string&, std::string_view, Stat&>;
			if constexpr (std::is_same_v<ResultType, void>) {
				return [=]<typename... Params>(Params&&... params) {
					action(std::forward<Params>(params)...);
					return true; // continue directory-traversal
				};
			} else {
				return action;
			}
		}

		template<typename FileAction, typename DirAction>
		bool foreach_dirent(std::string& path, FileAction fileAction, DirAction dirAction) {
			auto [invokeFile, statFile] = adaptParams(fileAction);
			auto [invokeDir,  statDir ] = adaptParams(dirAction);
			auto invokeFileAction = adaptReturn(invokeFile);
			auto invokeDirAction  = adaptReturn(invokeDir);
			bool needStat = statFile || statDir;

			ReadDir dir(path);
			bool addSlash = !path.empty() && (path.back() != '/');
			if (addSlash) path += '/';
			auto origLen = path.size();
			while (dirent* d = dir.getEntry()) {
				std::string_view f(d->d_name);
				if (f == one_of(".", "..")) continue;
				path += f;
				auto file = std::string_view(path).substr(origLen);

				// When the action is not interested in 'Stat' and on operation systems
				// that support it: avoid doing an extra 'stat()' system call.
				auto type = needStat ? static_cast<unsigned char>(DT_UNKNOWN) : d->d_type;
				if (type == DT_REG) {
					Stat dummy;
					if (!invokeFileAction(path, file, dummy)) {
						return false; // aborted
					}
				} else if (type == DT_DIR) {
					Stat dummy;
					if (!invokeDirAction(path, file, dummy)) {
						return false;
					}
				} else {
					if (auto st = getStat(path)) {
						if (isRegularFile(*st)) {
							if (!invokeFileAction(path, file, *st)) {
								return false;
							}
						} else if (isDirectory(*st)) {
							if (!invokeDirAction(path, file, *st)) {
								return false;
							}
						}
					}
				}

				path.resize(origLen);
			}
			if (addSlash) path.pop_back();

			return true; // finished normally
		}
	}

	template<typename FileAction>
	bool foreach_file(std::string path, FileAction fileAction)
	{
		auto dirAction = [&](const std::string& /*dirPath*/) { /*nothing*/ };
		return detail::foreach_dirent(path, fileAction, dirAction);
	}

	template<typename FileAction, typename DirAction>
	bool foreach_file_and_directory(std::string path, FileAction fileAction, DirAction dirAction)
	{
		return detail::foreach_dirent(path, fileAction, dirAction);
	}

	template<typename FileAction>
	bool foreach_file_recursive(std::string path, FileAction fileAction)
	{
		std::function<bool(std::string&)> dirAction;
		dirAction = [&](std::string& dirPath) {
			return detail::foreach_dirent(dirPath, fileAction, dirAction);
		};
		return dirAction(path);
	}

} // namespace openmsx

#endif
