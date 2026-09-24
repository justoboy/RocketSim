# RocketSim C++ core — plan index

This repo (`D:/RLBotTraining/rocketsim`) is the RocketSim C++ simulation core (a clone of
`https://github.com/ZealanL/RocketSim`). Active plans for it:

- **T13 — Native Ball Attach (C++):** add `GameMode::SPIKE_RUSH`/`GRIDIRON` + a `BallState.AttachInfo`
  weld (rotating offset, `ω × offset` release, serialize-safe attach state). Requires CMake build +
  wheel rebuild.
  [T13 - Native Ball Attach (C++).md](./T13%20-%20Native%20Ball%20Attach%20(C++).md)
- **T14 — Dropshot Exposure:** the core ALREADY implements dropshot natively; this plan binds the
  existing `GameMode.DROPSHOT` / `DropshotTilesState` / `Ball.DropshotInfo` in the pybind layer and
  routes it through rlgym. No core change; wheel rebuild.
  [T14 - Dropshot Exposure (bindings + rlgym routing).md](./T14%20-%20Dropshot%20Exposure%20(bindings%20+%20rlgym%20routing).md)

These are the C++/wheel-rebuild track. They run in parallel with the custombot (T9/T12/T15) and
rlgym (T10/T11) Python tracks. The custombot master index lives at
`D:/RLBotTraining/custombot/.kilo/plans/Deferred Game-Mode Work - Remaining Tasks and Order.md`.

**Before editing this repo: read the
[Fork & Backup Protocol](D:/RLBotTraining/custombot/.kilo/plans/Fork%20&%20Backup%20Protocol.md)** —
capture `git remote -v`/`status`/`diff`, report divergence vs upstream, get the user's fork URL,
push an initial backup commit, only then edit source. Never rewrite history; never push to upstream.
