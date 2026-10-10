#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>

void ImplementationGuardHit(const char* pFile, int line)
{
	static std::mutex mutex;
	static std::set<std::pair<std::string, int>> seen;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (seen.insert({ pFile, line }).second) {
			fprintf(stderr, "[guard] %s:%d\n", pFile, line);
			fflush(stderr);
		}
	}

	static const bool bContinue = getenv("KYA_GUARD_LOG") != nullptr;
	if (!bContinue) {
		assert(false);
	}
}
