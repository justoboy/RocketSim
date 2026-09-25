Track C (rocketsim) is fully implemented, built, and pushed. Commit `6588eb1` on `origin/main` (fork `justoboy/RocketSim`) contains the v2 ball-attach rework.

## What changed (rocketsim core only)

**Data model**
- [`CarControls.h`](src/Sim/CarControls.h): added `bool powerup` (rumble button) + serialization.
- [`Ball.h`](src/Sim/Ball/Ball.h): `AttachInfo` now carries `attachedCarId`, persistent `lastCarrierId` (survives release for rlgym scoring), `localOffset`, `engageTimer`, `releaseCooldown`, `active` (activation gate) — all serialized.
- [`RLConst.h`](src/RLConst.h): `SpikeRush` (ACTIVATION_DELAY 2s, RELEASE_COOLDOWN, MIN_ATTACH_TIME), `Gridiron` (ROOF_LOCAL_OFFSET, REACQUIRE_COOLDOWN, WALL_FUMBLE_Z, throw tuning, football shape), shared `ATTACH_INVULN_TIME = 0.5f`, and GRIDIRON 4v4 spawn tables.
- [`MutatorConfig.cpp`](src/Sim/MutatorConfig/MutatorConfig.cpp): GRIDIRON uses dropshot-style recharge.

**Mechanics**
- [`Ball.cpp`](src/Sim/Ball/Ball.cpp): football (prolate-spheroid) collision shape; `_OnAttach` = collision-based engage/steal (free→weld at contact point for Spike Rush / roof offset for Gridiron; steal respects the 0.5s invuln; Spike Rush opponent steal demos the carrier); `_PreTickUpdate` kinematic weld (`ω × offset` point-velocity), powerup release, double-jump fumble, flip-lob/dodge-spiral throws, wall-ride fumble, re-acquire lockout.
- [`Arena.cpp`](src/Sim/Arena/Arena.cpp): car-ball callback routes through `_OnAttach`; car-car callback handles carrier-bump steal (Spike Rush demo+transfer) / fumble (Gridiron), teammate bump = no-op; carrier boost locked to 0 in `Step`; GRIDIRON kickoff-after-goal hands the ball to the conceding team (tracked via `_lastGoalTeam`/`_hasScoredOnce`).
- [`Framework.h`](src/Framework.h): `RS_VERSION` → `2.2.3` (both `BallState` and `CarControls` changed).

## Cross-repo exposure (documented in T16 handoff, not implemented here)
- Bindings track: expose `GameMode.SPIKE_RUSH/GRIDIRON`, `CarControls.powerup`, `BallState.attach_info` (incl. `last_carrier_id`), football shape; rebuild wheel.
- rlgym track: route both modes, map the release action onto `powerup`, compute 7/3/own-goal + first-to-50 from `last_carrier_id`, feed carrier/attach state into obs.

## Verification
Core library compiled cleanly (19/19 objects, `RocketSim.lib` linked, exit 0). Soccar/hoops/heatseeker/snowday paths are byte-unchanged; attach logic is gated behind `attachInfo.active`. No custombot/rlgym source was touched — only `.kilo/plans/` files in this repo.