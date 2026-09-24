# T16 — Handoff: expose native ball-attach (T13) + dropshot (T14) through the pybind wheel and rlgym

Status: **NEW — handoff to the bindings/pybind repo and the rlgym fork.** This file documents the
*cross-repo* work required to consume the C++ core changes I (Agent C) already landed in
`D:/RLBotTraining/rocketsim`. The pybind binding layer and rlgym are **outside the rocketsim core
repo**, so they are NOT edited here — this plan is the contract for the other tracks.

## What the rocketsim core now provides (already implemented + built)

The core library (`RocketSim.lib`) compiles and links clean at version **2.2.2**. The new public
surface the bindings must mirror:

### GameMode (src/Sim/GameMode.h)
- `GameMode::SPIKE_RUSH` and `GameMode::GRIDIRON` were **appended after `THE_VOID`** so the byte
  values of the pre-existing modes (soccar=0 … void=5) are unchanged. New values: `spike_rush = 6`,
  `gridiron = 7`. `GAMEMODE_STRS[]` gained `"spike_rush"`, `"gridiron"`.
- `GameMode::DROPSHOT` already existed (value 4, string `"dropshot"`).

### BallState (src/Sim/Ball/Ball.h)
New `BallState.AttachInfo` struct, **already serialized** in `BALLSTATE_SERIALIZATION_FIELDS`:
```cpp
struct AttachInfo {
    uint32_t attachedCarId = 0;   // 0 = free ball
    Vec      localOffset   = {};   // carrier-local offset (rotates with the car)
    float    engageTimer   = 0;    // seconds attached to current carrier
    float    releaseCooldown = 0;  // seconds before re-attach allowed
};
AttachInfo attachInfo;
```
- `BallState::attachInfo` is the spike-rush/gridiron weld state. It survives clone/serialize like
  `hsInfo`/`dsInfo` already do.
- `DropshotInfo dsInfo` (chargeLevel, accumulatedHitForce, yTargetDir, hasDamaged, lastDamageTick)
  is already serialized (T14 core side — no change needed).

### Behavior (src/Sim/Ball/Ball.cpp, src/Sim/Arena/Arena.cpp)
- While `attachInfo.attachedCarId != 0`, the ball is kinematically welded: world pos =
  `carrier.pos + carrier.rotMat × localOffset`, world vel = `carrier.vel + carrier.angVel × offset`
  (rigid-body point velocity, so a spinning car whips the puck). Auto-engages when a non-demoed
  car is within `RLConst::SpikeRush::ATTACH_RADIUS`; releases on the carrier's jump (after
  `MIN_ATTACH_TIME`); carrier demo/destroy also releases.
- `IsBallScored()` / `IsBallProbablyGoingIn()` treat SPIKE_RUSH/GRIDIRON like soccar goals.

## Bindings repo (pybind → `RocketSim` python module) — TODO for that track

1. **Locate + fork the pybind repo** that produces the installed `RocketSim` wheel (it is NOT the
   rocketsim core clone and NOT rlgym). Per the Fork & Backup Protocol, capture `git remote -v`,
   report divergence, get the user's fork URL, push a backup commit, then edit.
2. **Bind the new enum members**: `GameMode.SPIKE_RUSH`, `GameMode.GRIDIRON` (and confirm
   `GameMode.DROPSHOT` is bound — T14).
3. **Bind `BallState.attach_info`** mirroring how `hsInfo`/`dsInfo` are surfaced today:
   `attached_car_id: int`, `local_offset: Vec`, `engage_timer: float`, `release_cooldown: float`.
4. **Bind `DropshotTilesState`/`DropshotTileState`** (140 tile damage states) and the
   `Ball.DropshotInfo` fields (T14) if not already exposed.
5. **Rebuild the wheel** against the v2.2.2 core and **install into `D:/RLBotTraining/custombot/.venv`**:
   `D:/RLBotTraining/custombot/.venv/Scripts/python.exe -m pip install <rebuilt wheel>`.
   NOTE: the wheel must be built from THIS core (the justoboy/RocketSim fork at v2.2.2), otherwise
   the field-count check in `MutatorConfig::Deserialize` / `BallState::Deserialize` will reject it.

## rlgym fork — TODO for that track (T11 router integration)

- `RocketSimEngine.set_mode(SPIKE_RUSH)` / `set_mode(GRIDIRON)` route to an arena built with the new
  `GameMode`; `_get_state`/`set_state` carry `attach_info` (attached car id + local offset + timers)
  through `GameState`/`PhysicsObject` so a mid-carry snapshot/restore round-trips.
- `set_mode(DROPSHOT)` routes to the dropshot arena; the 140 tile states + `dsInfo` round-trip
  through `GameState` (defaults = all FULL / inactive).
- Feeding the 140 tile states and the attach state into the network is a custombot obs design problem
  (T15), isolated from this handoff.

## Verification (after the wheel is rebuilt + installed)

- Build a spike_rush arena via the wheel; drive a car into the free puck; assert it welds and that
  flipping the car keeps the puck on the roof (rotating offset), not a fixed world z.
- Release while the car spins; assert the puck inherits the `ω × offset` tangential velocity.
- Serialize a mid-carry arena → deserialize → assert `attached_car_id` and `local_offset` survive.
- Dropshot probe: hit a tile, assert FULL→DAMAGED→BROKEN and `goal_scored` fires behind a broken tile.
- soccar/hoops/heatseeker/snowday behavior byte-identical to v2.2.1 (all additions are additive;
  existing enum byte values unchanged).

## Out of scope for rocketsim (Agent C)

The pybind binding layer and rlgym glue are separate repos. I will not scaffold or edit them. This
plan is the interface contract; the bindings/rlgym tracks own steps 1–5 above and the rlgym router.
