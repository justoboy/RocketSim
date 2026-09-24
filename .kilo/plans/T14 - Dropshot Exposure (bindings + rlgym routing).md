# T14 — Dropshot Exposure: bind the already-native dropshot + route it through rlgym

Status: NEW actionable plan. **No C++ core change needed** — the RocketSim C++ core ALREADY fully
implements dropshot. The only gap is that the **Python binding layer** (the pybind project that
generates the installed `RocketSim` module / `RocketSim.pyi`) does not bind it, so rlgym can't reach
it. This plan = extend the bindings, rebuild the wheel, and route dropshot through rlgym's engine.

## Correction to the old "out of scope" note

Dropshot was previously marked "formally cut." That was WRONG. It is fully implemented in the core;
it was only unreachable because the wheel's `GameMode` enum omits `DROPSHOT` and `BallState` omits
`dsInfo`/tile state. The local core source == the installed 2.2.1 wheel (we have NOT modified either
repo yet). This plan re-opens dropshot as available.

## What the core already provides (verified, do NOT re-implement)

- `D:/RLBotTraining/rocketsim/src/Sim/GameMode.h`: `GameMode::DROPSHOT` already in the enum +
  `"dropshot"` in `GAMEMODE_STRS[]`.
- `D:/RLBotTraining/rocketsim/src/Sim/Arena/DropshotTiles/DropshotTiles.h`: `DropshotTileState`
  (`STATE_FULL/STATE_DAMAGED/STATE_BROKEN`) and `DropshotTilesState` = `states[2][70]`
  (`TEAM_AMOUNT=2`, `NUM_TILES_PER_TEAM=70` → 140 tiles).
- `D:/RLBotTraining/rocketsim/src/Sim/Ball/Ball.h`: `Ball.DropshotInfo { int chargeLevel; float
  accumulatedHitForce; float yTargetDir; bool hasDamaged; uint64_t lastDamageTick; }` — and these
  fields are ALREADY in `BALLSTATE_SERIALIZATION_FIELDS`.
- `D:/RLBotTraining/rocketsim/src/Sim/Arena/Arena.h`: `GetDropshotTilesState()` /
  `SetDropshotTilesState()` already exist; dropshot tile collision + scoring handled in `Arena.cpp`.

## The actual gap (binding layer + rlgym)

1. **Bindings repo (open item: locate + fork it).** The pybind project that produces the `RocketSim`
   python package currently registers `GameMode` as only `SOCCAR/HOOPS/HEATSEEKER/SNOWDAY/THE_VOID`
   and does not bind `DropshotTilesState` or `Ball.DropshotInfo`. Pre-flight: identify that repo
   (it is NOT the `rocketsim` core clone and NOT `rlgym`), report its URL to the user, fork it, and
   per [Fork & Backup Protocol](D:/RLBotTraining/custombot/.kilo/plans/Fork%20&%20Backup%20Protocol.md)
   push an initial backup commit.
2. **Bindings: expose dropshot.** Bind `GameMode.DROPSHOT`, a `DropshotTileState`/`DropshotTilesState`
   python type (140 damage states), and the `Ball.DropshotInfo` fields on `BallState` (mirror how
   `hsInfo` is already surfaced as `heatseeker_*`).
3. **Rebuild + install the wheel** into `D:/RLBotTraining/custombot/.venv` (this is the same wheel
   rebuild as T13; sequence the two C++/binding rebuilds together).
4. **rlgym glue: route dropshot.** In the rlgym fork's `RocketSimEngine`, the T11 multi-arena router
   builds an `rsim.Arena(GameMode.DROPSHOT, ...)`; `_get_state`/`set_state` carry the 140 tile states
   + `dsInfo` through `GameState`/`PhysicsObject` (defaults = all FULL / inactive).

## Verification

- Probe: build a dropshot arena via the rebuilt wheel, hit a tile, assert its state flips
  FULL→DAMAGED→BROKEN and that `goal_scored` fires when the ball reaches the open net behind a
  broken tile; assert `chargeLevel`/`yTargetDir` behave.
- rlgym: `set_mode(DROPSHOT)` routes to the dropshot arena; tile state round-trips through
  `GameState` snapshot/restore.
- soccar/hoops/heatseeker/snowday byte-identical (dropshot additions are additive).

## Out of scope (→ T15)

Feeding the 140 tile states into the network is a custombot obs design problem, isolated in
[T15 - Dropshot Tile Inputs (custombot)](D:/RLBotTraining/custombot/.kilo/plans/T15%20-%20Dropshot%20Tile%20Inputs%20(custombot).md).
