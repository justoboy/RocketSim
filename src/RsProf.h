#pragma once
// TEMPORARY profiling instrumentation (perf engagement) - gated behind RS_PROF.
// Standalone header so Arena.cpp can define the accumulators. Inert unless RS_PROF defined.
// Uses QPC + volatile storage to survive -O3 (chrono::steady_clock is optimized away).
#ifdef RS_PROF
#include <cstdio>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace RocketSim { namespace RSProf {
	struct Acc { volatile double us = 0; volatile uint64_t n = 0; };
	// Arena::Step level
	extern Acc carsPre, padsPre, ballPre, bullet, carPost, padsCheck, padsPost, ballPost, scored;
	inline void AccAdd(Acc& a, double us) { a.us += us; a.n++; }
	inline LONGLONG Now() { LONGLONG t; QueryPerformanceCounter((LARGE_INTEGER*)&t); return t; }
	inline double QpcUs(LONGLONG a, LONGLONG b) {
		static volatile LONGLONG freq = [](){ LONGLONG f; QueryPerformanceFrequency((LARGE_INTEGER*)&f); return f; }();
		return (double)(b - a) / (double)freq * 1e6;
	}
	void Dump();
} }

// Statement form: fully sequenced (capture now, accumulate, then re-stamp the timestamp).
#define RS_PROF_ACC(acc, t0) do { LONGLONG _rsn = ::RocketSim::RSProf::Now(); ::RocketSim::RSProf::AccAdd(acc, ::RocketSim::RSProf::QpcUs(t0, _rsn)); t0 = _rsn; } while (0)
#define RS_PROF_T() ::RocketSim::RSProf::Now()
#else
#define RS_PROF_ACC(acc, t0)
#define RS_PROF_T() 0
#endif
