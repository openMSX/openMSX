/*
 *  PCL by Davide Libenzi ( Portable Coroutine Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#include <cassert>
#include <cstdlib>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "probed_defs.hh"
#include "pcl.hh"


#if defined(HAVE_GETCONTEXT) && defined(HAVE_MAKECONTEXT) && defined(HAVE_SWAPCONTEXT)

// Use this if the system has a working getcontext/makecontext/swapcontext
// implementation.
#define CO_USE_UCONEXT

#elif defined(HAVE_SIGACTION)

// Use this to have the generic signal implementation ( not working on
// Windows ). Suggested on generic Unix implementations or on Linux with
// CPU different from x86 family.
#define CO_USE_SIGCONTEXT

// Use this in conjuction with CO_USE_SIGCONTEXT to use the sigaltstack
// environment ( suggested when CO_USE_SIGCONTEXT is defined ).
#ifdef HAVE_SIGALTSTACK
#define CO_HAS_SIGALTSTACK
#endif

#endif // HAVE_GETCONTEXT ...

#ifdef CO_USE_UCONEXT
#include <ucontext.h>
typedef ucontext_t co_core_ctx_t;
#else
#include <setjmp.h>
typedef jmp_buf co_core_ctx_t;
#endif

#ifdef CO_USE_SIGCONTEXT
#include <signal.h>
#endif


struct co_ctx_t {
	co_core_ctx_t cc;
};

struct coroutine {
	co_ctx_t ctx;
	int alloc;
	struct coroutine* caller;
	struct coroutine* restarget;
	entry_func* func;
	void *data;
};

// The following value must be power of two ( 2^N ).
static const unsigned CO_STK_ALIGN = 256;
static const unsigned CO_STK_MIN = 4096;
static const unsigned CO_MIN_SIZE = sizeof(coroutine) + CO_STK_MIN;



static coroutine co_main;
static coroutine* co_curr = &co_main;
static coroutine* co_dhelper;

#ifdef CO_USE_SIGCONTEXT

static volatile bool ctx_called;
static co_ctx_t *ctx_creating;
static void *ctx_creating_func;
static sigset_t ctx_creating_sigs;
static co_ctx_t ctx_trampoline;
static co_ctx_t ctx_caller;

#endif // CO_USE_SIGCONTEXT



#ifndef CO_HAS_SIGALTSTACK
#ifdef CO_HAS_SIGSTACK

static int co_ctx_sdir(unsigned long psp)
{
	int nav = 0;
	unsigned long csp = (unsigned long)&nav;
	return (psp > csp) ? -1: +1;
}

static int co_ctx_stackdir(void)
{
	int cav = 0;
	return co_ctx_sdir((unsigned long)&cav);
}

#endif
#endif


#ifdef CO_USE_UCONEXT

static bool co_set_context(co_ctx_t* ctx, entry_func* func, char* stkbase, long stksiz)
{
	if (getcontext(&ctx->cc)) {
		return false;
	}
 
	ctx->cc.uc_link = NULL;
 
	ctx->cc.uc_stack.ss_sp = stkbase;
	ctx->cc.uc_stack.ss_size = stksiz - sizeof(int);
	ctx->cc.uc_stack.ss_flags = 0;
 
	makecontext(&ctx->cc, (void(*)())func, 1);

	return true;
}


static void co_switch_context(co_ctx_t *octx, co_ctx_t *nctx) {

	if (swapcontext(&octx->cc, &nctx->cc) < 0) {
		assert(false); // [PCL] Context switch failed
	}
}

#else // CO_USE_UCONEXT

#ifdef CO_USE_SIGCONTEXT

/*
 * This code comes from the GNU Pth implementation and uses the
 * sigstack/sigaltstack() trick.
 *
 * The ingenious fact is that this variant runs really on _all_ POSIX
 * compliant systems without special platform kludges.  But be _VERY_
 * carefully when you change something in the following code. The slightest
 * change or reordering can lead to horribly broken code.  Really every
 * function call in the following case is intended to be how it is, doubt
 * me...
 *
 * For more details we strongly recommend you to read the companion
 * paper ``Portable Multithreading -- The Signal Stack Trick for
 * User-Space Thread Creation'' from Ralf S. Engelschall.
 */

static void co_ctx_bootstrap()
{
	co_ctx_t* volatile ctx_starting;
	void (* volatile ctx_starting_func)();
 
	// Switch to the final signal mask (inherited from parent)
	sigprocmask(SIG_SETMASK, &ctx_creating_sigs, NULL);
 
	// Move startup details from static storage to local auto
	// variables which is necessary because it has to survive in
	// a local context until the thread is scheduled for real.
	ctx_starting = ctx_creating;
	ctx_starting_func = (void (*)(void)) ctx_creating_func;
 
	// Save current machine state (on new stack) and
	// go back to caller until we're scheduled for real...
	if (!setjmp(ctx_starting->cc)) {
		longjmp(ctx_caller.cc, 1);
	}

	// The new thread is now running: GREAT!
	// Now we just invoke its init function....
	ctx_starting_func();
	assert(false); // Hmm, you really shouldn't reach this point
}

static void co_ctx_trampoline(int sig)
{
	// Save current machine state and _immediately_ go back with
	// a standard "return" (to stop the signal handler situation)
	// to let him remove the stack again. Notice that we really
	// have do a normal "return" here, or the OS would consider
	// the thread to be running on a signal stack which isn't
	// good (for instance it wouldn't allow us to spawn a thread
	// from within a thread, etc.)
	if (!setjmp(ctx_trampoline.cc)) {
		ctx_called = true;
		return;
	}
 
	// Ok, the caller has longjmp'ed back to us, so now prepare
	// us for the real machine state switching. We have to jump
	// into another function here to get a new stack context for
	// the auto variables (which have to be auto-variables
	// because the start of the thread happens later).
	co_ctx_bootstrap();
}


static bool co_set_context(co_ctx_t* ctx, entry_func* func, char* stkbase, long stksiz)
{
	struct sigaction sa;
	struct sigaction osa;
	sigset_t osigs;
	sigset_t sigs;
#ifdef CO_HAS_SIGSTACK
	struct sigstack ss;
	struct sigstack oss;
#elif defined(CO_HAS_SIGALTSTACK)
	struct sigaltstack ss;
	struct sigaltstack oss;
#else
#error "PCL: Unknown context stack type"
#endif

	// Preserve the SIGUSR1 signal state, block SIGUSR1,
	// and establish our signal handler. The signal will
	// later transfer control onto the signal stack.
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
	sa.sa_handler = co_ctx_trampoline;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if (sigaction(SIGUSR1, &sa, &osa) != 0) {
		return false;
	}

	// Set the new stack.
	//
	// For sigaltstack we're lucky [from sigaltstack(2) on
	// FreeBSD 3.1]: ``Signal stacks are automatically adjusted
	// for the direction of stack growth and alignment
	// requirements''
	//
	// For sigstack we have to decide ourself [from sigstack(2)
	// on Solaris 2.6]: ``The direction of stack growth is not
	// indicated in the historical definition of struct sigstack.
	// The only way to portably establish a stack pointer is for
	// the application to determine stack growth direction.''
#ifdef CO_HAS_SIGALTSTACK
	ss.ss_sp = stkbase;
	ss.ss_size = stksiz - sizeof(int);
	ss.ss_flags = 0;
	if (sigaltstack(&ss, &oss) < 0) {
		return false;
	}
#elif defined(CO_HAS_SIGSTACK)
	if (co_ctx_stackdir() < 0) {
		ss.ss_sp = (stkbase + stksiz - sizeof(int));
	} else {
		ss.ss_sp = stkbase;
	}
	ss.ss_onstack = 0;
	if (sigstack(&ss, &oss) < 0) {
		return false;
	}
#else
#error "PCL: Unknown context stack type"
#endif

	// Now transfer control onto the signal stack and set it up.
	// It will return immediately via "return" after the setjmp()
	// was performed. Be careful here with race conditions.  The
	// signal can be delivered the first time sigsuspend() is
	// called.
	ctx_called = false;
	kill(getpid(), SIGUSR1);
	sigfillset(&sigs);
	sigdelset(&sigs, SIGUSR1);
	while (!ctx_called) {
		sigsuspend(&sigs);
	}

	// Inform the system that we are back off the signal stack by
	// removing the alternative signal stack. Be careful here: It
	// first has to be disabled, before it can be removed.
#ifdef CO_HAS_SIGALTSTACK
	sigaltstack(NULL, &ss);
	ss.ss_flags = SS_DISABLE;
	if (sigaltstack(&ss, NULL) < 0) {
		return false;
	}
	sigaltstack(NULL, &ss);
	if (!(ss.ss_flags & SS_DISABLE)) {
		return false;
	}
	if (!(oss.ss_flags & SS_DISABLE)) {
		sigaltstack(&oss, NULL);
	}
#elif defined(CO_HAS_SIGSTACK)
	if (sigstack(&oss, NULL)) {
		return false;
	}
#else
#error "PCL: Unknown context stack type"
#endif

	// Restore the old SIGUSR1 signal handler and mask
	sigaction(SIGUSR1, &osa, NULL);
	sigprocmask(SIG_SETMASK, &osigs, NULL);

	// Set creation information.
	ctx_creating = ctx;
	ctx_starting_func = (void*)func;
	memcpy(&ctx_creating_sigs, &osigs, sizeof(sigset_t));

	// Now enter the trampoline again, but this time not as a signal
	// handler. Instead we jump into it directly.
	if (!setjmp(ctx_caller.cc)) {
		longjmp(ctx_trampoline.cc, 1);
	}
	return true;
}

#else // CO_USE_SIGCONTEXT

static bool co_set_context(co_ctx_t *ctx, entry_func* func, char *stkbase, long stksiz)
{
	char* stack = stkbase + stksiz - sizeof(int);

	setjmp(ctx->cc);

#if defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
    && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(JB_PC) && defined(JB_SP)
	ctx->cc[0].__jmpbuf[JB_PC] = (int) func;
	ctx->cc[0].__jmpbuf[JB_SP] = (int) stack;
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
    && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(__mc68000__)
	ctx->cc[0].__jmpbuf[0].__aregs[0] = (long) func;
	ctx->cc[0].__jmpbuf[0].__sp = (int *) stack;
#elif defined(__GNU_LIBRARY__) && defined(__i386__)
	ctx->cc[0].__jmpbuf[0].__pc = func;
	ctx->cc[0].__jmpbuf[0].__sp = stack;
#elif defined(_WIN32) && defined(_MSC_VER)
	((_JUMP_BUFFER *) &ctx->cc)->Eip = (long) func;
	((_JUMP_BUFFER *) &ctx->cc)->Esp = (long) stack;
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
    && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && (defined(__powerpc64__) || defined(__powerpc__))
	ctx->cc[0].__jmpbuf[JB_LR] = (int) func;
	ctx->cc[0].__jmpbuf[JB_GPR1] = (int) stack;
#else
#error "PCL: Unsupported setjmp/longjmp platform. Please report to <davidel@xmailserver.org>"
#endif

	return true;
}

#endif // CO_USE_SIGCONTEXT


static void co_switch_context(co_ctx_t *octx, co_ctx_t *nctx)
{
	if (!setjmp(octx->cc)) {
		longjmp(nctx->cc, 1);
	}
}

#endif // CO_USE_UCONEXT


static void co_runner(void* /*dummy*/)
{
	coroutine* co = co_curr;
	co->restarget = co->caller;
	co->func(co->data);
	co_exit();
}

coroutine* co_create(entry_func* func, void* data, void* stack, unsigned size)
{
	size &= ~(sizeof(int) - 1);
	assert(size >= CO_MIN_SIZE);

	int alloc = 0;
	if (!stack) {
		size = (size + CO_STK_ALIGN - 1) & ~(CO_STK_ALIGN - 1);
		stack = malloc(size);
		alloc = size;
	}
	coroutine* co = (coroutine*)stack;
	co->alloc = alloc;
	co->func = func;
	co->data = data;
	if (!co_set_context(&co->ctx, co_runner, (char*)stack, size)) {
		if (alloc) {
			free(co);
		}
		return NULL;
	}
	return co;
}

void co_delete(coroutine* co)
{
	assert(co != co_curr); // Cannot delete itself
	if (co->alloc) {
		free(co);
	}
}

void co_call(coroutine* co)
{
	coroutine* oldco = co_curr;
	co->caller = co_curr;
	co_curr = co;
	co_switch_context(&oldco->ctx, &co->ctx);
}

void co_resume()
{
	co_call(co_curr->restarget);
	co_curr->restarget = co_curr->caller;
}

static void co_del_helper(void* /*data*/)
{
	while (true) {
		coroutine* cdh = co_dhelper;
		co_dhelper = NULL;
		co_delete(co_curr->caller);
		co_call(cdh);
		assert(co_dhelper); // Resume to delete helper coroutine
	}
}

void co_exit_to(coroutine* co)
{
	static coroutine* dchelper = NULL;
	static char stk[CO_MIN_SIZE];

	if (!dchelper &&
	    !(dchelper = (coroutine*)co_create(co_del_helper, NULL, stk, sizeof(stk)))) {
		assert(false); // Unable to create delete helper coroutine
	}

	co_dhelper = co;
	co_call(dchelper);
	assert(false); // Stale coroutine called
}

void co_exit(void)
{
	co_exit_to(co_curr->restarget);
}

coroutine* co_current(void)
{
	return co_curr;
}

