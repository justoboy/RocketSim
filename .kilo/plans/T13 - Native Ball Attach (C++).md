# T13 — Native Ball-Attach (C++): Spike Rush weld + Gridiron roof-attach in RocketSim core

Status: NEW plan. **This is the C++/CLion track.** This file is designed to be MOVED into
`D:\RLBotTraining\rocketsim\.kilo\plans\` and executed from the CLion IDE against the C++ repo.
It is SELF-CONTAINED: all paths below are absolute; it does not depend on reading any custombot
plan file.

## Why native (not the Python weld)

The custombot Python weld (`D:/RLBotTraining/custombot/custombot/spike_rush.py`) has concrete
accuracy gaps vs. a native implementation:
- attach offset is world-space `carrier.pos + (0,0,z)` — a flipped car doesn't keep the puck glued
  to its roof; native must use `car.rotMat × local_offset` so the puck rotates with the car.
- release velocity is `ball.vel = carrier.vel` (no rotational term); native must apply
  `car.vel + ω × offset` (rigid-body point velocity) so a spinning car whips the puck correctly.
- attach state lives in the engine instance, so it is LOST when the caller snapshots/restores a
  mid-carry `GameState`; native stores it in `BallState` so it survives clone/serialize like
  `hsInfo` already does.

## Fork & backup (MANDATORY before any edit)

`D:/RLBotTraining/rocketsim` is a local clone of upstream `https://github.com/ZealanL/RocketSim`.
Before editing:
1. `git -C D:/RLBotTraining/rocketsim remote -v` → capture current `origin`.
2. `git -C D:/RLBotTraining/rocketsim status` and `git -C D:/RLBotTraining/rocketsim log --oneline -5`
   and `git -C D:/RLBotTraining/rocketsim diff --stat` → detect ANY existing local divergence.
3. Report findings to the user; ask them to fork upstream and supply their fork URL.
4. Then per repo: `git remote rename origin upstream`; `git remote add origin <USER_FORK_URL>`;
   `git fetch upstream && git merge upstream/main`; initial
   `git add -A && git commit -m "Backup: pre-T13 snapshot" && git push -u origin <branch>`.
5. Only after the backup commit is pushed, make source changes. Never rewrite history; never push
   to `upstream`.

## Build / wheel pipeline (this track DOES rebuild the wheel)

- Configure + build with CMake/CLion (Release, C++20): `D:/RLBotTraining/rocketsim/CMakeLists.txt`.
- Regenerate + install the Python wheel into the custombot venv:
  `D:/RLBotTraining/custombot/.venv/Scripts/python.exe -m pip install <rebuilt wheel>`.
- Bump the serialization macro alongside any `BallState`/`MutatorConfig` field change, or
  `Deserialize` hard-fails on field-count mismatch.

## C++ changes

1. **`D:/RLBotTraining/rocketsim/src/Sim/GameMode.h`**: add `SPIKE_RUSH` (and `GRIDIRON`) to
   `enum class GameMode` and matching strings to `GAMEMODE_STRS[]`.
2. **`D:/RLBotTraining/rocketsim/src/Sim/Ball/Ball.h`**: add an `AttachInfo` struct to `BallState`
   (`uint32_t attachedCarId = 0; Vec localOffset; float engageTimer; float releaseCooldown;`) and
   add the new fields to `BALLSTATE_SERIALIZATION_FIELDS`.
3. **`D:/RLBotTraining/rocketsim/src/Sim/Ball/Ball.cpp`**: in `_PreTickUpdate`, add a
   `SPIKE_RUSH`/`GRIDIRON` branch mirroring the existing `HEATSEEKER` kinematic-override block:
   while attached, set ball pos = `car.rotMat × localOffset + car.pos` and ball vel =
   `car.vel + car.angVel × worldOffset`; auto-engage when a car is within attach radius of a free
   ball; release on the carrier's jump (apply `car.vel + ω × offset`); steal-on-demo via the existing
   car-bump path.
4. **`D:/RLBotTraining/rocketsim/src/Sim/Arena/Arena.cpp`**: extend `Arena::Create` /
   `ResetToRandomKickoff` spawn tables for the new modes (spike rush = soccar field; gridiron =
   4v4 spawn table).
5. **`D:/RLBotTraining/rocketsim/src/RocketSim.cpp`**: in `GetArenaCollisionShapes`, map
   `SPIKE_RUSH`/`GRIDIRON` → soccar mesh (no unique mesh), same pattern as the existing
   `SNOWDAY`/`HEATSEEKER → SOCCAR` fallthrough.

## Python binding surface (regenerated wheel must expose)

- `BallState.attach_info` fields (attached car id, local offset, timers) so the rlgym glue can
  read/write them across the sim→obs boundary.
- `GameMode.SPIKE_RUSH` / `GameMode.GRIDIRON` enum members.

## Verification (C++ side)

- Unit probe (C++ or via the rebuilt wheel): attach a ball to a car, rotate the car upside-down,
  assert the puck follows the roof (rotating offset), not a fixed world z.
- Release while spinning: assert the puck inherits the `ω × offset` tangential velocity.
- Serialize a mid-carry arena → deserialize → assert `attachedCarId` and offset survive.
- Soccar/hoops/heatseeker/snowday behavior byte-identical to before the enum additions.

## Handoff back to custombot (Python, separate)

Once the rebuilt wheel is installed, the custombot side (T11's engine routing + obs) reads
`attach_info` from `BallState` instead of `SpikeRushEngine._attached_id`. That Python wiring lives
in the custombot plans, not here.
