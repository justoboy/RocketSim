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
