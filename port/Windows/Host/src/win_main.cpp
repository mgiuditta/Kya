#include "kya.h"
#include "renderer.h"
#include "DebugMenu.h"
#include "DebugRenderer.h"

#include "port/input.h"
#include "gamepad.h"
#include "Texture.h"
#include "Mesh.h"
#include "edSysTransferService.h"

#include <csignal>
#include "log.h"

#ifdef __APPLE__
#include <execinfo.h>
#include <unistd.h>
#endif

void signal_handler(int signal)
{
	Log::GetInstance().ForceFlush();
}

#ifdef __APPLE__
// Print a backtrace to stderr on a crash; the macOS app sends stderr to last_run.log.
static void crash_handler(int signal)
{
	void* frames[64];
	const int count = backtrace(frames, 64);
	const char header[] = "\nFatal signal, backtrace:\n";
	write(STDERR_FILENO, header, sizeof(header) - 1);
	backtrace_symbols_fd(frames, count, STDERR_FILENO);
	// Best effort: the game logs are buffered and _exit would drop them.
	Log::GetInstance().ForceFlush();
	// Exit instead of re-raising so macOS does not pop a crash report dialog.
	_exit(128 + signal);
}
#endif

int main(int argc, char** argv) {
	std::signal(SIGINT, signal_handler);
#ifdef __APPLE__
	std::signal(SIGSEGV, crash_handler);
	std::signal(SIGBUS, crash_handler);
	// Release builds turn proven undefined behaviour into brk (SIGTRAP).
	std::signal(SIGTRAP, crash_handler);
	std::signal(SIGILL, crash_handler);
	std::signal(SIGABRT, crash_handler);
	std::signal(SIGFPE, crash_handler);
#endif

	DebugMenu::ApplyStartupSettings();
	Renderer::Setup();
	DebugMenu::SetupRenderer();
	Renderer::Native::Setup();
	DebugMenu::Init();
	Renderer::Kya::TextureLibrary::Init();
	Renderer::Kya::MeshLibrary::Init();
	DebugMenu::AddKeyboardMouseSupport();
	KyaGamepad::AddGamepadSupport();
	main_internal(argc, argv);
	Audio::Shutdown();
}
