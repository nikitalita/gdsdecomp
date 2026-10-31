/**************************************************************************/
/*  tsan_sigaction_wrapper.cpp                                            */
/**************************************************************************/

#if defined(TSAN_ENABLED) && defined(__unix__)
#include <dlfcn.h>
#include <signal.h>

extern "C" int __interceptor_sigaction(int p_sig, const struct sigaction *p_act, struct sigaction *p_oldact);

// NativeAOT uses SIGRTMIN on Linux (or SIGUSR1 on platforms where SIGRTMIN is unavailable) as
// INJECT_ACTIVATION_SIGNAL to suspend managed threads for GC and inspects/modifies the interrupted
// thread's kernel ucontext_t and stack return addresses. TSAN's sigaction interceptor wraps signal
// handlers and defers asynchronous signals into a copied ucontext_t buffer (ProcessPendingSignalsImpl),
// which corrupts NativeAOT's stack walking and return-address hijacking. Bypass TSAN's sigaction
// interceptor for SIGRTMIN and SIGUSR1.
extern "C" __attribute__((visibility("default"), used)) int sigaction(int p_sig, const struct sigaction *p_act, struct sigaction *p_oldact) {
	if (
#ifdef SIGRTMIN
			p_sig == SIGRTMIN ||
#endif
			p_sig == SIGUSR1) {
		using SigactionFn = int (*)(int, const struct sigaction *, struct sigaction *);
		static SigactionFn real_sigaction = nullptr;
		if (real_sigaction == nullptr) {
			real_sigaction = reinterpret_cast<SigactionFn>(dlsym(RTLD_NEXT, "sigaction"));
		}
		if (real_sigaction != nullptr) {
			return real_sigaction(p_sig, p_act, p_oldact);
		}
	}
	return __interceptor_sigaction(p_sig, p_act, p_oldact);
}

void godot_mono_decomp_tsan_sigaction_force_link() {}
#endif
