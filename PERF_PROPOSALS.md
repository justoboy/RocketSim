# PERF_PROPOSALS.md — RocketSim engine perf engagement

State file for the perf engagement on RocketSim (this repo), the engine behind
RLGymSim_CPP. Re-read this file before acting on any future task.

## Engagement baseline (fixed: single env, 2000 steps, seed 12345, MinGW -O3)

- Golden checksums (must stay byte-identical):
  - obsSum = 660377.2520941221
  - rewSum = 1208.1670994163
- Baseline throughput (3 repeats, full-step harness): 15,797 / 18,943 / 16,619 steps/s
  → median ~16.6k steps/s; noise floor ±5% (spread is ~19% peak-to-peak, so real
  wins must be large or proven by construction)
- Physics-only path: ~26.6k steps/s (~4.5µs/tick)
- Per-subsystem profile (RS_PROF build, single car):
  - bullet (stepSimulation internals): ~2.80 µs/tick (~60%)
  - carsPre (updateVehicleFirst: 4× wheel transforms, 4× suspension raycasts,
    friction impulses, jump/boost logic): ~1.66 µs/tick (~35%)
  - everything else (padsPre/ballPre/carPost/padsPost/ballPost/scored): <2% combined

## Open

- [ ] Multi-threaded `stepSimulation` (parallelize solver/island/integration, or batch
  envs across threads) | why Tier B: floating-point accumulation order in
  `solveConstraints` and island processing changes → observable numerics diverge from
  custombot | expected gain: up to ~Nx on multi-env | effort: high | risk: HIGH
  (correctness guarantee broken) | rollback: single commit | recommendation: DECLINE —
  guardrail 1 forbids it
- [ ] SIMD/`-ffast-math`/FMA-contraction build changes to btVector3 & solver math |
  why Tier B: intentionally alters numerics (FMA contraction, FTZ, reassociation) |
  expected gain: 1.2–2× on bullet path | effort: medium | risk: HIGH | rollback: single
  commit | recommendation: DECLINE — numerics change is the point, which violates
  no-divergence
- [ ] Swap broadphase (btRSBroadphase ↔ btDbvtBroadphase) or reorder pair cache |
  why Tier B: contact-manifold ordering changes → impulse accumulation order changes →
  divergent physics | expected gain: unknown | effort: medium | risk: HIGH |
  recommendation: DECLINE

## Approved - implementing

- (none)

## Done

- [x] RS_PROF-gated per-subsystem timing accumulators in Arena::Step | shipped 2026-11-12 |
  inert in production builds (macro expands to nothing unless -DRS_PROF=1); enabled the
  profile above. Profiler itself fixed twice (chrono DCE'd by -O3; unsequenced
  read/write of timestamp made all deltas zero).

## Declined

- [x] Dedup `updateWheelTransformsWS(wheel)` inside `btVehicleRL::rayCast` (already
  computed by the preceding `updateWheelTransform(i)` loop) | declined 2026-11-13 |
  provably byte-identical (checksums matched) but measured gain below the ±5% noise
  floor (after: 17.6/15.9/16.1k vs baseline 15.8/18.9/16.6k — overlapping spreads);
  reverted in 5652aec/05684c8. Do not re-raise.
- [x] FMA contraction (`-mfma -ffp-contract=fast`) | user-approved 2026-11-13, tested,
  then REVERTED — measured on an identical at-rest state: no-FMA median ~53,400 vs FMA
  ~51,700 physics-only steps/s (wash, ~3% slower, inside noise). Tick is latency-bound
  (raycasts + branchy solver), not FLOP-bound. Drift was as expected (seed 12345:
  obsSum 660377.25→668814.56). Zero speedup for a numerics change = strictly negative
  trade; reverted dd9d5c2 (main) / b3d3bf5 (cpp-standalone). Do not re-raise.

## Conclusion

Tier A (provably-identical, measurable) is **exhausted** on this repo. The tick is
~95% Bullet internals + car per-tick vehicle math; the only redundant work found was
sub-noise. Every remaining lever (threading, SIMD/FTZ/FMA, broadphase swap) changes
floating-point ordering or numerics and therefore violates the no-divergence guardrail.
Per protocol: "this is fine, don't optimize it" is the honest finding for the remaining
candidates. The wrapper's ~88% physics share can only be reduced by accepting a
numerics change (Tier B above) — that is the user's decision, not an autonomous edit.

## Thread-scaling measurement (2026-11-13, scratch.cpp harness)

Question: does moving parallelization into rocketsim core help? Answer: no — env-level
parallelism is already embarrassingly parallel and already done at the learner layer
(ThreadAgent/ThreadManager). Relocating the std::thread loop into rocketsim changes the
owner, not the speed. The only *single-env* win would be Bullet's intra-step parallel
solver (btParallelConstraintSolver + btTaskScheduler, vendored but unused), which is a
numerics change (Tier B) gated on "learns just as well".

Measured: 16 independent SOCCAR arenas (1 car each, fixed kickoff, zero controls =>
at-rest deterministic), sharded across T std::threads, stepped in lockstep. Aggregate
physics ticks/s vs T (min-of-5, i9-10850K 20 logical cores):

  T=1   ~300k ticks/s
  T=2   ~560k   (~1.9x)
  T=4   ~1.16M (~3.8x)
  T=8   ~1.98M (~6.6x, knee)
  T=16  ~2.5M  (plateau)
  T=20  ~2.3M  (plateau / slight decline)

Honesty caveat: the at-rest per-tick cost is ~3µs, so thread-spawn + scheduler +
frequency-scaling noise DOMINATES absolute numbers (single-thread reference disagreed
with the 1-thread sweep entry by 4.6x on identical work; per-config spread 3-55%). The
*shape* is reliable (near-linear to ~8, plateau ~16); the exact optimum is not.

Recommendation for the learner (RLGymPPO_CPP examplemain.cpp):
- Physics is cheap and saturates ~8-16 workers. The learner's real SPS ceiling is torch
  policy inference + obs build on the collection threads, NOT raw physics.
- Size numThreads*numGamesPerThread to (logical cores minus reserve for torch). On a
  20-core box the example's 4x4=16 is already in the plateau; raising it further buys
  nothing and steals cores from torch. Lowering to ~12 frees headroom if torch-bound.
- Do NOT migrate the thread loop into rocketsim: same work, same speed, more coupling.
