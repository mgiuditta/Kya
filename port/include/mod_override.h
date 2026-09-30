#pragma once

// Loose-file mod override: a file at mods/<path> replaces the game's copy of <path>.
// <path> is the disc path (CDEURO/LEVEL/...) for whole files and streams, or a bank entry's
// name-tree path without its drive (Projects/BWITCH/Resource/Build/...) for bank entries.
// ponytail: matching follows the host filesystem's case rules (insensitive on Windows/macOS, sensitive on Linux).

#include <filesystem>
#include <string>
#include <system_error>

inline bool ModOverridesPresent()
{
	static const bool bPresent = [] {
		std::error_code error;
		return std::filesystem::is_directory("mods", error);
	}();
	return bPresent;
}

// Returns the override file for the given game path, or an empty string when there is none.
inline std::string ModOverridePath(const char* path)
{
	if (!ModOverridesPresent() || path == nullptr) {
		return {};
	}

	// Drop a device or drive prefix ("cdrom0:", "D:") and leading separators.
	for (const char* c = path; *c != '\0' && *c != '\\' && *c != '/'; c++) {
		if (*c == ':') {
			path = c + 1;
			break;
		}
	}
	while (*path == '\\' || *path == '/') {
		path++;
	}

	std::string candidate = "mods/";
	for (const char* c = path; *c != '\0' && *c != ';'; c++) {
		candidate.push_back(*c == '\\' ? '/' : *c);
	}

	std::error_code error;
	return std::filesystem::is_regular_file(candidate, error) ? candidate : std::string();
}
