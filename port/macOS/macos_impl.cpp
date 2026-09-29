#include <port.h>
#include <ctime>

void sceScfGetLocalTimefromRTC(sceCdCLOCK* localTime)
{
	const std::time_t now = std::time(nullptr);
	std::tm local = {};
	localtime_r(&now, &local);
	localTime->second = local.tm_sec;
}
