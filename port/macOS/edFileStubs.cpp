// ponytail: stand-ins for edFile code upstream hasn't pushed yet (edFile b8e68f1
// declares these but doesn't define them). Delete when the submodule has them.
#include "edFile/edFile.h"
#include "edFile/ps2/_edFileFilerMCard.h"

#include <cstring>

// Frees the handle slot, like the CLOSE action does for files opened without 0x20.
bool edFileReleaseHandle(edFILEH* pFile)
{
	for (int i = 0; i < 0x10; i++) {
		if (&edFileHandleData[i] == pFile) {
			edFileHandleTable[i] = 0;
			memset(&edFileHandleData[i], 0, sizeof(edFILEH));
			return true;
		}
	}
	return false;
}

bool edCFiler_MemoryCard::cmdbreak()
{
	return false;
}
