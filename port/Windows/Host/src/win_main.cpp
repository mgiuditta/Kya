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
#include <dlfcn.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>
#endif

void signal_handler(int signal)
{
	Log::GetInstance().ForceFlush();
}

#ifdef __APPLE__
// Print a backtrace to stderr on a crash; the macOS app sends stderr to last_run.log.
static void crash_handler(int signal, siginfo_t* pInfo, void* pContext)
{
	void* frames[64];
	const int count = backtrace(frames, 64);
	char header[256];
	const ucontext_t* pUc = static_cast<const ucontext_t*>(pContext);
	// backtrace() walks frame pointers and loses the faulting function itself; print pc/lr too.
	Dl_info pcInfo{};
	const uintptr_t pc = pUc ? pUc->uc_mcontext->__ss.__pc : 0;
	const uintptr_t lr = pUc ? pUc->uc_mcontext->__ss.__lr : 0;
	dladdr(reinterpret_cast<void*>(pc), &pcInfo);
	const int len = snprintf(header, sizeof(header), "\nFatal signal %d at %p (fault addr %p), pc %s+%lu, lr %p\nbacktrace:\n",
		signal, reinterpret_cast<void*>(pc), pInfo ? pInfo->si_addr : nullptr,
		pcInfo.dli_sname ? pcInfo.dli_sname : "?", pcInfo.dli_saddr ? (unsigned long)(pc - reinterpret_cast<uintptr_t>(pcInfo.dli_saddr)) : 0ul,
		reinterpret_cast<void*>(lr));
	write(STDERR_FILENO, header, len > 0 ? len : 0);
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
	{
		struct sigaction action {};
		action.sa_sigaction = crash_handler;
		action.sa_flags = SA_SIGINFO;
		// Release builds turn proven undefined behaviour into brk (SIGTRAP).
		for (const int signal : { SIGSEGV, SIGBUS, SIGTRAP, SIGILL, SIGABRT, SIGFPE }) {
			sigaction(signal, &action, nullptr);
		}
	}
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
