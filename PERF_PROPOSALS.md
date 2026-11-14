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

## custombot (Python reference) env/thread config — comparison (2026-11-13)

User recalled "32 was the best number of envs in the python version." Confirmed in the
custombot repo. This is the SAME learner shape (batched forward + lockstep env stepping),
but the bottleneck that SETS the optimal env count is different, so the number does not
port directly.

### What custombot actually used
- `train_rl_ab2.py` defaults: `--n-envs 32` and `--matches-per-epoch 32` (coupled equal).
- `custombot/train_config.py` `n_envs: 32`; rollout primitive is
  `LeagueRollout.run_episode_multi` (custombot/rollout.py): steps n_envs episodes in
  lockstep, ONE grouped forward per decision step (batch = 2*n_envs), then row-slices.
- `tick_skip = 8` (8 physics ticks per decision step) — same 120Hz->15Hz decimation as ours.
- The coupled sweep over {8,16,32,64} (warmup-discard, shuffled order, 3 reps) found n=32
  the consistent peak; n=64 regressed.

### WHY 32 was the peak there (the key difference)
custombot's BOTTLENECK_ANALYSIS.md is explicit: "Collection is dominated by per-step GPU
launch/sync overhead, NOT physics: bare env.step with NO model runs 366-632 decision-
steps/s, but the batch-2 forward path runs only 89-107 steps/s." In the Python stack the
physics is cheap relative to per-step Python + per-step GPU launch/sync cost. Batching
more envs into ONE grouped forward amortizes that fixed per-step overhead across all envs,
so throughput climbs with n_envs until the wide forward (2*n_envs rows) + the n_envs-wide
physics step per tick themselves become the limiter (~32). Past 32 the wide forward +
physics step dominate and it turns over.

### What actually sets the optimal env count (corrected framing)
The env count that maximizes throughput is set by WHICH cost is the bottleneck:
1. Per-step FIXED overhead (Python glue + one GPU launch + device sync per decision step).
   Paid once per step regardless of envs sharing that step. Batching more envs into one
   forward amortizes it => more envs = faster => HIGH optimum. This is exactly why Python
   peaked at 32: BOTTLENECK_ANALYSIS.md shows bare env.step (no model) = 366-632 steps/s but
   the batch-2 forward path = only 89-107. The forward/overhead, NOT physics, is the wall.
2. Physics FLOPs themselves. If physics is the real cost, more envs can't beat core count,
   so optimum ~= core count.

Python's optimum is high (32) because bottleneck (1) dominates there; physics is not the
limiter, so envs are the lever.

### Honest status of the C++ learner's optimum (NOT measured)
I do NOT have a measured C++ learner optimum. The "8-16" figure above is PHYSICS-ONLY thread
scaling (scratch.cpp): it proves physics saturates at ~core count, but it does NOT prove the
learner's optimum is 16. The C++ learner still runs a per-step torch forward. If that forward
is the C++ learner's real bottleneck (the same role the GPU forward played in Python), then
MORE envs would help C++ too, exactly like Python, and the C++ optimum could also be ~32.
Earlier drafts of this file claimed "C++ optimum is bounded by the physics-core knee (~16)";
that was an OVER-CLAIM. Physics-only saturation != learner SPS optimum. The Python 32 came
from a real coupled sweep on the real loop; I have no equivalent sweep on the C++ learner.

### Actionable conclusion
- To know the C++ learner's real optimum, run the SAME coupled sweep on the real learner:
  vary numThreads * numGamesPerThread, watch reported SPS, find the knee. That is the only
  honest way to set it; do not copy 32 OR 16 by assumption.
- Whether more envs helps C++ depends on whether C++ is forward-bound (then yes, like Python)
  or physics-bound (then no, capped by cores). That split is unmeasured on the real learner.
- rocketsim itself: no change warranted. Tier A exhausted; the env/thread-count lever lives
  in the learner, not the engine.
