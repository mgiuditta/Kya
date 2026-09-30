#pragma once

// ponytail: stand-in for the edFile header upstream hasn't pushed yet; every save
// write fails cleanly. Delete this Shim folder when the edFile submodule has it.
#include <filesystem>
#include <fstream>

class WinSaveFile
{
public:
	explicit WinSaveFile(const std::filesystem::path&, unsigned = 0) {}
	bool IsOpen() const { return false; }
	bool Write(const void*, unsigned) { return false; }
	bool Commit() { return false; }
};
