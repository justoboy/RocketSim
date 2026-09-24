# RocketSim C++ core — plan index

This repo (`D:/RLBotTraining/rocketsim`) is the RocketSim C++ simulation core (a clone of
`https://github.com/ZealanL/RocketSim`). Active plans for it:

- **T13 — Native Ball Attach (C++) [v2 REWORK]:** Spike Rush and Gridiron are **distinct** modes.
  Spike Rush = soccar 3v3, spikes activate 2s after kickoff, touch attaches at contact point, boost
  locked while carrying, `CarControls.powerup` releases, touching the carrier instantly demos+steals.
  Gridiron = 4v4 football, roof-attach, no release button (double-jump fumble / flip lob / dodge
  spiral), steal-on-touch (0.5s invuln), opponent-bump fumble, wall-ride fumble, 2s re-acquire
  lockout, kickoff-after-goal attaches to conceding team, first-to-50. Adds `BallState.AttachInfo`
  (`attachedCarId`, `lastCarrierId`, `localOffset`, timers) + `CarControls.powerup`. Requires CMake
  build + wheel rebuild. **Source rework NOT yet implemented** (v1 identical-mechanic code is in
  place at v2.2.2; v2 spec below is implementation-ready for a code-capable mode).
  [T13 - Native Ball Attach (C++).md](./T13%20-%20Native%20Ball%20Attach%20(C++).md)
- **T14 — Dropshot Exposure:** the core ALREADY implements dropshot natively; this plan binds the
  existing `GameMode.DROPSHOT` / `DropshotTilesState` / `Ball.DropshotInfo` in the pybind layer and
  routes it through rlgym. No core change; wheel rebuild.
  [T14 - Dropshot Exposure (bindings + rlgym routing).md](./T14%20-%20Dropshot%20Exposure%20(bindings%20+%20rlgym%20routing).md)
- **T16 — Handoff (bindings + rlgym):** the interface contract for the *other* repos to consume the
  T13/T14 core changes — bind `GameMode.SPIKE_RUSH`/`GRIDIRON` + `BallState.attach_info` + dropshot
  tile state in the pybind wheel, rebuild/install into the custombot venv, and route through rlgym.
  (Owned by the bindings/rlgym tracks, not the rocketsim core.)
  [T16 - Ball-Attach & Dropshot Bindings + rlgym Routing (handoff).md](./T16%20-%20Ball-Attach%20%26%20Dropshot%20Bindings%20+%20rlgym%20Routing%20(handoff).md)

**Status (Agent C / rocketsim core):** T13 v1 (identical-mechanic sticky puck) is in place at v2.2.2;
the **v2 rework** (distinct Spike Rush/Gridiron mechanics, `CarControls.powerup`, `lastCarrierId`,
football shape, 2s arming, steal/fumble/invuln/lockout rules) is **specified and implementation-ready
but not yet coded** — needs a code-capable mode. T14 confirmed already-native (no core change). The
wheel rebuild + rlgym routing are cross-repo and captured in T16.

These are the C++/wheel-rebuild track. They run in parallel with the custombot (T9/T12/T15) and
rlgym (T10/T11) Python tracks. The custombot master index lives at
`D:/RLBotTraining/custombot/.kilo/plans/Deferred Game-Mode Work - Remaining Tasks and Order.md`.

**Before editing this repo: read the
[Fork & Backup Protocol](D:/RLBotTraining/custombot/.kilo/plans/Fork%20&%20Backup%20Protocol.md)** —
capture `git remote -v`/`status`/`diff`, report divergence vs upstream, get the user's fork URL,
push an initial backup commit, only then edit source. Never rewrite history; never push to upstream.
