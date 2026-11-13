// scratch.cpp — multi-env physics scaling benchmark for RocketSim (pre-authorized scratch space).
//
// Purpose: measure AGGREGATE physics throughput (sim-ticks/s) as a function of how many
// independent Arena instances are stepped across how many std::threads. This mirrors what
// the learner's ThreadAgent/ThreadAgentManager does internally (N games sharded across T
// threads, each thread serially stepping its assigned games). The output is the scaling
// curve that informs the learner's numThreads / numGamesPerThread config.
//
// This file is pure instrumentation: it does NOT change any library source. It links against
// the same RocketSim sources the wrapper builds, so the per-tick physics work is identical.
//
// Determinism note: each arena uses a fixed kickoff seed and zero controls, so every arena
// is in the same at-rest state every run. The checksum (sum of all car/ball world positions
// across all arenas at the end) is a deterministic function of the sim, used to confirm the
// build is the same physics build across runs (bit-exact gate).

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <thread>
#include <algorithm>
#include <chrono>

#include "RocketSim.h"

using namespace RocketSim;

// Step every arena in [lo, hi) for 'ticks' ticks, serially within this function.
static void StepRange(std::vector<Arena*>& arenas, size_t lo, size_t hi, int ticks) {
	for (int t = 0; t < ticks; t++)
		for (size_t i = lo; i < hi; i++)
			arenas[i]->Step(1);
}

// Deterministic checksum over final state of all arenas (single car + ball position).
static double Checksum(const std::vector<Arena*>& arenas) {
	double s = 0.0;
	for (Arena* a : arenas) {
		for (Car* c : a->GetCars()) {
			btVector3 p = c->_rigidBody.getWorldTransform().getOrigin();
			s += (double)p.x() + (double)p.y() + (double)p.z();
		}
		btVector3 bp = a->ball->_rigidBody.getWorldTransform().getOrigin();
		s += (double)bp.x() + (double)bp.y() + (double)bp.z();
	}
	return s;
}

// Run one timed config: shard 'totalGames' arenas across 'T' threads, step each 'ticks' ticks.
// Returns aggregate ticks/s.
static double RunConfig(std::vector<Arena*>& arenas, int totalGames, int T, int ticks) {
	int gamesPerThread = totalGames / T;
	int remainder = totalGames % T;

	auto wall = std::chrono::steady_clock::now();
	std::vector<std::thread> ts;
	ts.reserve(T);
	size_t base = 0;
	for (int ti = 0; ti < T; ti++) {
		size_t lo = base;
		size_t hi = base + gamesPerThread + (ti < remainder ? 1 : 0);
		base = hi;
		ts.emplace_back([&, lo, hi]() {
			StepRange(arenas, lo, hi, ticks);
		});
	}
	for (auto& th : ts) th.join();
	auto wall2 = std::chrono::steady_clock::now();

	double secs = std::chrono::duration<double>(wall2 - wall).count();
	double totalTicks = (double)totalGames * (double)ticks;
	return totalTicks / secs;
}

int main(int argc, char** argv) {
	const int seed = (argc > 1) ? atoi(argv[1]) : 12345;
	const int ticksPerRun = (argc > 2) ? atoi(argv[2]) : 2000;
	const int repeats = (argc > 3) ? atoi(argv[3]) : 3;

	RocketSim::Init("D:/RLBotTraining/RLGymSim_CPP/collision_meshes", true);

	const int totalGames = 16; // matches the learner example's 16 games

	std::vector<Arena*> arenas;
	arenas.reserve(totalGames);
	for (int g = 0; g < totalGames; g++) {
		Arena* a = Arena::Create(GameMode::SOCCAR);
		a->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
		a->ResetToRandomKickoff(seed);
		arenas.push_back(a);
	}

	// Bit-exact gate: checksum after stepping arena 0 alone for the full tick count.
	{
		std::vector<Arena*> one{arenas[0]};
		StepRange(one, 0, 1, ticksPerRun);
		printf("[checksum] single-arena after %d ticks: %.10f\n", ticksPerRun, Checksum(one));
	}

	printf("cores=%u totalGames=%d ticksPerRun=%d seed=%d repeats=%d\n",
		(unsigned)std::thread::hardware_concurrency(), totalGames, ticksPerRun, seed, repeats);

	const int threadCounts[] = {1, 2, 4, 8, 10, 16, 20};
	const int nConfigs = sizeof(threadCounts) / sizeof(threadCounts[0]);

	// For each config, collect 'repeats' samples and report min (least-interfered) plus spread.
	// min is the headline metric for throughput: it's the run least contaminated by other system
	// work. The spread across repeats IS the noise floor — a real config change must beat it.

	// Capture single-thread min for speedup baseline.
	double singleMin = 0.0;
	{
		std::vector<double> samples;
		for (int r = 0; r < repeats; r++) {
			for (Arena* a : arenas) a->ResetToRandomKickoff(seed);
			samples.push_back(RunConfig(arenas, totalGames, 1, ticksPerRun));
		}
		std::sort(samples.begin(), samples.end());
		singleMin = samples.front();
	}
	printf("single-thread min reference: %.0f ticks/s\n", singleMin);

	printf("%-8s %-14s %-14s %-10s %-10s\n", "threads", "ticks/s(min)", "spread%", "speedup", "");
	for (int ci = 0; ci < nConfigs; ci++) {
		int T = threadCounts[ci];
		std::vector<double> samples;
		for (int r = 0; r < repeats; r++) {
			for (Arena* a : arenas) a->ResetToRandomKickoff(seed);
			samples.push_back(RunConfig(arenas, totalGames, T, ticksPerRun));
		}
		std::sort(samples.begin(), samples.end());
		double mn = samples.front();
		double mx = samples.back();
		double spread = 100.0 * (mx - mn) / mn;
		printf("%-8d %-14.0f %-10.1f %-10.2fx\n", T, mn, spread, mn / singleMin);
	}

	for (Arena* a : arenas) delete a;
	return 0;
}
