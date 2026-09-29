#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Forward dec.
typedef uint64_t DWORD64;

struct CallstackPreviewerEntry {
	CallstackPreviewerEntry(std::vector<DWORD64> inBacktrace)
		: backtrace(inBacktrace)
	{}

	CallstackPreviewerEntry()
	{}

	std::vector<DWORD64> backtrace;
	std::vector<std::string> callstack;
};

namespace CallstackPreviewer {
	void Show(CallstackPreviewerEntry& entry);
}