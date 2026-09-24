# T16 — Handoff: expose native ball-attach (T13) + dropshot (T14) through the pybind wheel and rlgym

Status: **HANDOFF — to the bindings/pybind repo and the rlgym fork.** This file documents the
*cross-repo* work required to consume the C++ core changes I (Agent C) land in
`D:/RLBotTraining/rocketsim` (T13 v2 rework). The pybind binding layer and rlgym are **outside the
rocketsim core repo**, so they are NOT edited here — this plan is the contract for the other tracks.

> **v2 update:** the first pass made Spike Rush and Gridiron identical. They are now **distinct**.
> Two new public-surface items the bindings MUST expose: `CarControls.powerup` (Spike Rush release
> button) and `BallState.attach_info.last_carrier_id` (persists after release, for rlgym scoring).

## What the rocketsim core provides (T13 v2)

Core library builds clean; `RS_VERSION` bumps to **2.2.3** (both `BallState` and `CarControls` change).

### GameMode (src/Sim/GameMode.h)
- `GameMode::SPIKE_RUSH` (=6) and `GameMode::GRIDIRON` (=7) appended after `THE_VOID`; existing byte
  values unchanged. `GAMEMODE_STRS[]` has `"spike_rush"`, `"gridiron"`. `DROPSHOT` already existed (4).

### CarControls (src/Sim/CarControls.h) — NEW FIELD
- `bool powerup;` added and serialized. **Spike Rush:** `powerup == true` releases an attached ball
  (no-op if not attached; you cannot pre-arm spikes). **Gridiron:** `powerup` is unused (release is
  double-jump fumble / flip lob / dodge spiral, driven by existing jump/flip inputs).

### BallState.AttachInfo (src/Sim/Ball/Ball.h) — already serialized
```cpp
struct AttachInfo {
    uint32_t attachedCarId = 0;   // 0 = free ball
    uint32_t lastCarrierId = 0;   // persists after release; rlgym uses this to attribute points
    Vec      localOffset   = {};   // carrier-local offset (rotates with the car)
    float    engageTimer   = 0;    // seconds attached to current carrier
    float    releaseCooldown = 0;  // seconds before re-attach allowed (2s lockout)
};
```
- `lastCarrierId` is the "last car that held the ball" value rlgym reads at the goal tick.

### Behavior (src/Sim/Ball/Ball.cpp, src/Sim/Arena/Arena.cpp)
- **Spike Rush:** inactive until 2s after kickoff; then touch attaches at the **contact point**;
  carrier boost locked to 0 and pads don't refill; `powerup` releases keeping momentum (spinning
  carrier whips the puck via `ω×offset`); touching the carrier **instantly demos** them and transfers
  the ball; soccar goal scoring.
- **Gridiron:** 4v4; **roof-attach** (fixed offset, not contact point); football (prolate) shape;
  NO release button — double-jump fumbles, forward flip lobs, sideways dodge spirals; any other
  player touching carrier/ball steals (unless within 0.5s invuln); opponent bump fumbles; wall-ride
  above the line fumbles; 2s re-acquire lockout; boost locked while carrying, recharge-when-free
  (dropshot-style); after a goal the next kickoff attaches to the conceding team's car.
- `IsBallScored()` / `IsBallProbablyGoingIn()` treat SPIKE_RUSH/GRIDIRON like soccar goals.
- Core keeps **no score / no match-end** (same as heatseeker). rlgym owns 7/3/own-goal + first-to-50.

## Bindings repo (pybind → `RocketSim` python module) — TODO for that track

1. **Locate + fork the pybind repo** that produces the installed `RocketSim` wheel (NOT the rocketsim
   core clone, NOT rlgym). Per the Fork & Backup Protocol: capture `git remote -v`, report divergence,
   get the user's fork URL, push a backup commit, then edit.
2. **Bind the new enum members**: `GameMode.SPIKE_RUSH`, `GameMode.GRIDIRON` (confirm `DROPSHOT` bound).
3. **Bind `CarControls.powerup`** (the new release/powerup boolean) alongside the existing controls.
4. **Bind `BallState.attach_info`**: `attached_car_id`, `last_carrier_id`, `local_offset`,
   `engage_timer`, `release_cooldown`.
5. **Bind `DropshotTilesState`/`DropshotTileState`** (140 tile states) + `Ball.DropshotInfo` (T14).
6. **Rebuild the wheel** against the v2.2.3 core and **install into `D:/RLBotTraining/custombot/.venv`**:
   `D:/RLBotTraining/custombot/.venv/Scripts/python.exe -m pip install <rebuilt wheel>`. The wheel MUST
   be built from THIS core (justoboy/RocketSim @ 2.2.3) or the field-count check in
   `MutatorConfig::Deserialize`/`BallState::Deserialize`/`CarControls` will reject it.

## rlgym fork — TODO for that track (router integration)

- `set_mode(SPIKE_RUSH)` / `set_mode(GRIDIRON)` / `set_mode(DROPSHOT)` route to the right arena;
  `_get_state`/`set_state` carry `attach_info` (incl. `last_carrier_id`) and `powerup` through
  `GameState`/`PhysicsObject` so a mid-carry snapshot/restore round-trips.
- **Action space:** Spike Rush maps its release action onto `CarControls.powerup`. Gridiron has NO
  release button — its release is driven by the existing jump (double-jump fumble) and flip/dodge
  inputs. Do not wire `powerup` for Gridiron.
- **Scoring:** at each goal tick read `attach_info.last_carrier_id`; carried-by-scorer → 7, released
  → 3, own goal → 3. Core fires only the team-only goal callback.
- **Match-end:** first to 50 (Gridiron), like heatseeker's first-to-7. Core does not track score.
- Feeding tile states + attach state into the network is a custombot obs design problem (T15).

## Verification (after the wheel is rebuilt + installed)

- Spike Rush: before 2s ball bounces normally; after 2s touch welds at contact point; flipping keeps
  puck on roof; `powerup` releases keeping momentum; spinning carrier whips the puck; touching the
  carrier demos them and transfers the ball; carrier boost stays 0 and pads don't refill.
- Gridiron: roof-attach; single jump safe, double jump fumbles; forward flip lobs, right dodge
  spirals; steal on touch (unless 0.5s invuln); opponent bump fumbles; wall-ride fumble; 2s lockout;
  kickoff-after-goal attaches to conceding team; football tumbles.
- Serialize mid-carry → deserialize → `attached_car_id`/`last_carrier_id`/`local_offset` survive.
- soccar/hoops/heatseeker/snowday byte-identical to v2.2.1 (all additions additive).

## Out of scope for rocketsim (Agent C)

The pybind binding layer and rlgym glue are separate repos. I will not scaffold or edit them. This
plan is the interface contract; the bindings/rlgym tracks own the binding + router work above.
