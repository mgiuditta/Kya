#pragma once

#ifdef PLATFORM_WIN
#define ENABLE_MY_LOG
#else
#define uintptr_t int
//#define ENABLE_MY_LOG
#endif

#ifdef ENABLE_MY_LOG
#if defined(PLATFORM_WIN)
#define MY_LOG_CATEGORY(category, level, format, ...) do { if (Log::ShouldLog(level)) { Log::GetInstance().AddLog(level, category, format, ##__VA_ARGS__); } } while (0)
#define scePrintf(format, ...) MY_LOG_CATEGORY("PS2", LogLevel::Info, format, ##__VA_ARGS__)
#define MY_LOG(format, ...) MY_LOG_CATEGORY("General", LogLevel::Info, format, ##__VA_ARGS__)

#define FLUSH_LOG(...) Log::GetInstance().ForceFlush()


#else
#include <eekernel.h>
#define MY_LOG(...) scePrintf(##__VA_ARGS__); scePrintf("\n")
#define MY_LOG_CATEGORY(category, level, format, ...) scePrintf(format, ##__VA_ARGS__); scePrintf("\n")
#endif

#include <stdio.h>

#include <stdlib.h>

#else
#define MY_LOG(...)
#define MY_LOG_CATEGORY(...)
#define FLUSH_LOG(...)

#ifdef PLATFORM_WIN
#define scePrintf(...)
#endif
#endif