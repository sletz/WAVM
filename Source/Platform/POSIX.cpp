#ifndef _WIN32

#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Platform/Platform.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <string.h>

#include <sys/time.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef __APPLE__
    #define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef __linux__
	#include <execinfo.h>
	#include <dlfcn.h>
#endif

extern "C"
{
	extern void* __cxa_allocate_exception(size_t numBytes) throw();
	extern void __cxa_free_exception(void* exception) throw();
	extern void __cxa_throw(void* exception,void* exceptionTypeInfo,void (*dest)(void*));
	extern std::type_info* __cxa_current_exception_type();
}

namespace Platform
{
	static Uptr internalGetPreferredVirtualPageSizeLog2()
	{
		U32 preferredVirtualPageSize = sysconf(_SC_PAGESIZE);
		// Verify our assumption that the virtual page size is a power of two.
		assert(!(preferredVirtualPageSize & (preferredVirtualPageSize - 1)));
		return floorLogTwo(preferredVirtualPageSize);
	}
	Uptr getPageSizeLog2()
	{
		static Uptr preferredVirtualPageSizeLog2 = internalGetPreferredVirtualPageSizeLog2();
		return preferredVirtualPageSizeLog2;
	}
	
	U32 memoryAccessAsPOSIXFlag(MemoryAccess access)
	{
		switch(access)
		{
		default:
		case MemoryAccess::None: return PROT_NONE;
		case MemoryAccess::ReadOnly: return PROT_READ;
		case MemoryAccess::ReadWrite: return PROT_READ | PROT_WRITE;
		case MemoryAccess::Execute: return PROT_EXEC;
		case MemoryAccess::ReadWriteExecute: return PROT_EXEC | PROT_READ | PROT_WRITE;
		}
	}

	bool isPageAligned(U8* address)
	{
		const Uptr addressBits = reinterpret_cast<Uptr>(address);
		return (addressBits & ((1ull << getPageSizeLog2()) - 1)) == 0;
	}

	U8* allocateVirtualPages(Uptr numPages)
	{
		Uptr numBytes = numPages << getPageSizeLog2();
		auto result = mmap(nullptr,numBytes,PROT_NONE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(result == MAP_FAILED)
		{
			return nullptr;
		}
		return (U8*)result;
	}

	bool commitVirtualPages(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}
	
	bool setVirtualPageAccess(U8* baseVirtualAddress,Uptr numPages,MemoryAccess access)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		return mprotect(baseVirtualAddress,numPages << getPageSizeLog2(),memoryAccessAsPOSIXFlag(access)) == 0;
	}

	void decommitVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		auto numBytes = numPages << getPageSizeLog2();
		if(madvise(baseVirtualAddress,numBytes,MADV_DONTNEED)) { Errors::fatal("madvise failed"); }
		if(mprotect(baseVirtualAddress,numBytes,PROT_NONE)) { Errors::fatal("mprotect failed"); }
	}

	void freeVirtualPages(U8* baseVirtualAddress,Uptr numPages)
	{
		errorUnless(isPageAligned(baseVirtualAddress));
		if(munmap(baseVirtualAddress,numPages << getPageSizeLog2())) { Errors::fatal("munmap failed"); }
	}

	bool describeInstructionPointer(Uptr ip,std::string& outDescription)
	{
		#ifdef __linux__
			// Look up static symbol information for the address.
			Dl_info symbolInfo;
			if(dladdr((void*)ip,&symbolInfo) && symbolInfo.dli_sname)
			{
				outDescription = symbolInfo.dli_sname;
				return true;
			}
		#endif
		return false;
	}

	enum { signalStackNumBytes = 65536 };
	THREAD_LOCAL U8* signalStack = nullptr;
	THREAD_LOCAL U8* stackMinAddr = nullptr;
	THREAD_LOCAL U8* stackMaxAddr = nullptr;

	void initThread()
	{
		if(!signalStack)
		{
			// Allocate a stack to use when handling signals, so stack overflow can be handled safely.
			signalStack = new U8[signalStackNumBytes];
			stack_t signalStackInfo;
			signalStackInfo.ss_size = signalStackNumBytes;
			signalStackInfo.ss_sp = signalStack;
			signalStackInfo.ss_flags = 0;
			if(sigaltstack(&signalStackInfo,nullptr) < 0)
			{
				Errors::fatal("sigaltstack failed");
			}

			// Get the stack address from pthreads, but use getrlimit to find the maximum size of the stack instead of the current.
			struct rlimit stackLimit;
			getrlimit(RLIMIT_STACK,&stackLimit);
			#ifdef __linux__
				// Linux uses pthread_getattr_np/pthread_attr_getstack, and returns a pointer to the minimum address of the stack.
				pthread_attr_t threadAttributes;
				memset(&threadAttributes,0,sizeof(threadAttributes));
				pthread_getattr_np(pthread_self(),&threadAttributes);
				Uptr stackSize;
				pthread_attr_getstack(&threadAttributes,(void**)&stackMinAddr,&stackSize);
				pthread_attr_destroy(&threadAttributes);
				stackMaxAddr = stackMinAddr + stackSize;
				stackMinAddr = stackMaxAddr - stackLimit.rlim_cur;
			#else
				// MacOS uses pthread_getstackaddr_np, and returns a pointer to the maximum address of the stack.
				stackMaxAddr = (U8*)pthread_get_stackaddr_np(pthread_self());
				stackMinAddr = stackMaxAddr - stackLimit.rlim_cur;
			#endif

			// Include an extra page below the stack's usable address range to distinguish stack overflows from general SIGSEGV.
			const Uptr pageSize = sysconf(_SC_PAGESIZE);
			stackMinAddr -= pageSize;
		}
	}

	struct SignalContext
	{
		SignalContext* outerContext;
		jmp_buf returnEnv;
		SignalType outSignalType;
		void* outSignalData;
		CallStack outCallStack;
	};

	THREAD_LOCAL SignalContext* innermostSignalContext = nullptr;
	THREAD_LOCAL bool isReentrantSignal = false;

	[[noreturn]] void signalHandler(int signalNumber,siginfo_t* signalInfo,void*)
	{
		if(isReentrantSignal) { Errors::fatal("reentrant signal handler"); }
		isReentrantSignal = true;

		// If the signal occurred outside of a catchPlatformExceptions call, just treat it as a fatal error.
		if(!innermostSignalContext)
		{
			switch(signalNumber)
			{
			case SIGFPE: Errors::fatalf("unhandled SIGFPE\n");
			case SIGSEGV: Errors::fatalf("unhandled SIGSEGV\n");
			case SIGBUS: Errors::fatalf("unhandled SIGBUS\n");
			default: Errors::unreachable();
			};
		}

		// Derive the exception cause the from signal that was received.
		switch(signalNumber)
		{
		case SIGFPE:
			if(signalInfo->si_code != FPE_INTDIV && signalInfo->si_code != FPE_INTOVF) { Errors::fatal("unknown SIGFPE code"); }
			innermostSignalContext->outSignalType = SignalType::intDivideByZeroOrOverflow;
			break;
		case SIGSEGV:
		case SIGBUS:
		{
			innermostSignalContext->outSignalType
				= signalInfo->si_addr >= stackMinAddr && signalInfo->si_addr < stackMaxAddr
					? SignalType::stackOverflow
					: SignalType::accessViolation;
			auto accessViolationSignalData = new AccessViolationSignalData;
			accessViolationSignalData->address = reinterpret_cast<Uptr>(signalInfo->si_addr);
			innermostSignalContext->outSignalData = accessViolationSignalData;
			break;
		}
		default:
			Errors::fatalf("unknown signal number: %i",signalNumber);
			break;
		};

		// Capture the execution context, omitting this function and the function that called it,
		// so the top of the callstack is the function that triggered the signal.
		innermostSignalContext->outCallStack = captureCallStack(2);

		// Jump back to the setjmp in catchPlatformExceptions.
		siglongjmp(innermostSignalContext->returnEnv,1);
	}

	THREAD_LOCAL bool hasInitializedSignalHandlers = false;

	void initSignals()
	{
		if(!hasInitializedSignalHandlers)
		{
			hasInitializedSignalHandlers = true;

			// Set a signal handler for the signals we want to intercept.
			struct sigaction signalAction;
			signalAction.sa_sigaction = signalHandler;
			sigemptyset(&signalAction.sa_mask);
			signalAction.sa_flags = SA_SIGINFO | SA_ONSTACK;
			sigaction(SIGSEGV,&signalAction,nullptr);
			sigaction(SIGBUS,&signalAction,nullptr);
			sigaction(SIGFPE,&signalAction,nullptr);
		}
	}

	bool catchSignals(
		const std::function<void()>& thunk,
		const std::function<void(SignalType,void*,const CallStack&)>& handler
		)
	{
		initThread();
		initSignals();

		SignalContext signalContext;
		signalContext.outerContext = innermostSignalContext;
		signalContext.outSignalData = nullptr;
		signalContext.outSignalType = SignalType::accessViolation;

		// Use setjmp to allow signals to jump back to this point.
		bool isReturningFromSignalHandler = sigsetjmp(signalContext.returnEnv,1);
		if(!isReturningFromSignalHandler)
		{
			innermostSignalContext = &signalContext;

			// Call the thunk.
			thunk();
		}
		else
		{
			handler(
				innermostSignalContext->outSignalType,
				innermostSignalContext->outSignalData,
				innermostSignalContext->outCallStack
				);
		}

		innermostSignalContext = signalContext.outerContext;
		isReentrantSignal = false;

		return isReturningFromSignalHandler;
	}

	CallStack captureCallStack(Uptr numOmittedFramesFromTop)
	{
		#ifdef __linux__
			// Unwind the callstack.
			enum { maxCallStackSize = signalStackNumBytes / sizeof(void*) / 8 };
			void* callstackAddresses[maxCallStackSize];
			auto numCallStackEntries = backtrace(callstackAddresses,maxCallStackSize);

			// Copy the return pointers into the stack frames of the resulting ExecutionContext.
			// Skip the first numOmittedFramesFromTop+1 frames, which correspond to this function
			// and others that the caller would like to omit.
			CallStack result;
			for(Iptr index = numOmittedFramesFromTop + 1;index < numCallStackEntries;++index)
			{
				result.stackFrames.push_back({(Uptr)callstackAddresses[index]});
			}
			return result;
		#else
			return CallStack();
		#endif
	}

	struct PlatformException
	{
		void* data;
	};
	
	bool catchPlatformExceptions(
		const std::function<void()>& thunk,
		const std::function<void(void*)>& handler
		)
	{
		try
		{
			thunk();
			return false;
		}
		catch(PlatformException exception)
		{
			handler(exception.data);
			return true;
		}
	}
	
	void* allocateExceptionData(Uptr numBytes)
	{
		return __cxa_allocate_exception(numBytes);
	}
	
	std::type_info* getUserExceptionTypeInfo()
	{
		static std::type_info* typeInfo = nullptr;
		if(!typeInfo)
		{
			try
			{
				throw PlatformException {nullptr};
			}
			catch(PlatformException)
			{
				typeInfo = __cxa_current_exception_type();
			}
		}
		assert(typeInfo);
		return typeInfo;
	}

	[[noreturn]] void raisePlatformException(void* data)
	{
		throw PlatformException {data};
		Errors::unreachable();
	}

	U64 getMonotonicClock()
	{
		#ifdef __APPLE__
			timeval timeVal;
			gettimeofday(&timeVal, nullptr);
			return U64(timeVal.tv_sec) * 1000000 + U64(timeVal.tv_usec);
		#else
			timespec monotonicClock;
			clock_gettime(CLOCK_MONOTONIC,&monotonicClock);
			return U64(monotonicClock.tv_sec) * 1000000 + U64(monotonicClock.tv_nsec) / 1000;
		#endif
	}
	
	struct Mutex
	{
		pthread_mutex_t pthreadMutex;
	};

	Mutex* createMutex()
	{
		auto mutex = new Mutex();
		errorUnless(!pthread_mutex_init(&mutex->pthreadMutex,nullptr));
		return mutex;
	}

	void destroyMutex(Mutex* mutex)
	{
		errorUnless(!pthread_mutex_destroy(&mutex->pthreadMutex));
		delete mutex;
	}

	void lockMutex(Mutex* mutex)
	{
		errorUnless(!pthread_mutex_lock(&mutex->pthreadMutex));
	}

	void unlockMutex(Mutex* mutex)
	{
		errorUnless(!pthread_mutex_unlock(&mutex->pthreadMutex));
	}

	struct Event
	{
		pthread_cond_t conditionVariable;
		pthread_mutex_t mutex;
	};

	Event* createEvent()
	{
		auto event = new Event();

		pthread_condattr_t conditionVariableAttr;
		errorUnless(!pthread_condattr_init(&conditionVariableAttr));

		// Set the condition variable to use the monotonic clock for wait timeouts.
		#ifndef __APPLE__
			errorUnless(!pthread_condattr_setclock(&conditionVariableAttr,CLOCK_MONOTONIC));
		#endif

		errorUnless(!pthread_cond_init(&event->conditionVariable,nullptr));
		errorUnless(!pthread_mutex_init(&event->mutex,nullptr));

		errorUnless(!pthread_condattr_destroy(&conditionVariableAttr));

		return event;
	}

	void destroyEvent(Event* event)
	{
		pthread_cond_destroy(&event->conditionVariable);
		errorUnless(!pthread_mutex_destroy(&event->mutex));
		delete event;
	}
	
	bool waitForEvent(Event* event,U64 untilTime)
	{
		errorUnless(!pthread_mutex_lock(&event->mutex));

		int result;
		if(untilTime == UINT64_MAX)
		{
			result = pthread_cond_wait(&event->conditionVariable,&event->mutex);
		}
		else
		{
			timespec untilTimeSpec;
			untilTimeSpec.tv_sec = untilTime / 1000000;
			untilTimeSpec.tv_nsec = (untilTime % 1000000) * 1000;

			result = pthread_cond_timedwait(&event->conditionVariable,&event->mutex,&untilTimeSpec);
		}

		errorUnless(!pthread_mutex_unlock(&event->mutex));

		if(result == ETIMEDOUT)
		{
			return false;
		}
		else
		{
			errorUnless(!result);
			return true;
		}
	}

	void signalEvent(Event* event)
	{
		errorUnless(!pthread_cond_signal(&event->conditionVariable));
	}
}

#endif
