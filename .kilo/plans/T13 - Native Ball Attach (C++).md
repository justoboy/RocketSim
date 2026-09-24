# T13 — Native Ball-Attach (C++): Spike Rush + Gridiron in RocketSim core

Status: **REWORK (v2).** The first pass (commit `8d45a72`) treated Spike Rush and Gridiron as one
identical "sticky puck" mechanic. That was wrong: the two modes have **different** mechanics. This
file is the corrected, implementation-ready spec for the C++ core. It is SELF-CONTAINED; all paths
are absolute; it does not depend on reading any custombot plan file.

## Scope split (who owns what)

- **This plan (rocketsim core, Agent C):** the *simulation* of both modes — attach/engage/release,
  steal/demo, fumble, boost lock, pad gating, kickoff/spawn, ball shape, serialization.
- **Bindings repo (separate):** expose `GameMode.SPIKE_RUSH/GRIDIRON`, `CarControls.powerup`,
  `BallState.attach_info` (incl. `last_carrier_id`), and the football shape; rebuild + install wheel.
- **rlgym (separate):** route the two modes through the arena router; map the spike-rush release
  action onto `CarControls.powerup`; compute 7/3/own-goal points and "first to 50" from
  `attach_info.last_carrier_id`; feed carrier/attach state into obs. See T16 handoff.

## Decisions locked (from user Q&A)

1. **Release input = new `bool powerup` on `CarControls`.** It is the rumble powerup button. In
   Spike Rush its ONLY effect is to release an already-attached ball (you cannot pre-arm spikes).
   In Gridiron there is NO release button — release happens via double-jump fumble or flip/dodge
   throw. Name the field `powerup` (not `pickup`/`release`) because it is the shared rumble action.
2. **Core keeps NO score / match-end logic.** It only fires the existing
   `GoalScoreEventFn(this, scoringTeam, userInfo)` (see [`Arena.h`](src/Sim/Arena/Arena.h:24)),
   exactly like heatseeker. rlgym owns 7/3 weighting and "first to 50".
3. **rlgym reads a persistent "last carrier id"** to attribute points. So `AttachInfo` must keep a
   `lastCarrierId` that SURVIVES release (so a released/loose ball still knows who last held it).
4. **Boost lock + pad gating live in the core**, driven by the current carrier id, not in Python.

## Data-model changes

### `src/Sim/CarControls.h`
- Add `bool powerup;` to `struct CarControls` and to `CAR_CONTROLS_SERIALIZATION_FIELDS(name)`.

### `src/Sim/Ball/Ball.h` — `BallState.AttachInfo`
- Keep `attachedCarId` (0 = free ball). Add `uint32_t lastCarrierId` (persists after release; 0 if
  never held). Keep `localOffset`, `engageTimer`, `releaseCooldown`.
- Append `lastCarrierId` to `BALLSTATE_SERIALIZATION_FIELDS`.

### `src/RLConst.h`
- `SpikeRush`: `ATTACH_RADIUS` (already ~150), `ACTIVATION_DELAY = 2.0f` (spikes activate 2s after
  kickoff), `RELEASE_COOLDOWN = 2.0f` (no re-engage for 2s after release), `MIN_ATTACH_TIME` grace.
- `Gridiron`: `ROOF_OFFSET` (fixed local offset above roof, e.g. `Vec(0,0,CAR_ROOF_Z)`),
  `INVULN_TIME = 0.5f` (post-possession steal immunity), `REACQUIRE_COOLDOWN = 2.0f`,
  `WALL_FUMBLE_Z` (height line above goal top), `LOBBED_*`/`SPIRAL_*` throw tuning constants,
  `BALL_SHAPE = PROLATE` marker.

## Spike Rush mechanics (soccar field, 3v3)

Implement in [`Ball::_PreTickUpdate`](src/Sim/Ball/Ball.cpp:161) + [`Arena`](src/Sim/Arena/Arena.cpp):

1. **Arming delay:** attach mechanic is INACTIVE until `tickCount*tickTime >= SpikeRush::ACTIVATION_DELAY`
   (2s after kickoff). Before that, the ball behaves as a normal soccar ball (normal `_OnHit`).
2. **Engage on touch:** once active, when a non-demoed car's hitbox contacts the free ball, set
   `attachedCarId = car.id`, `lastCarrierId = car.id`, and capture `localOffset` = the **contact
   point** in the carrier's local frame (world→local via `rotMat` transpose). `engageTimer = 0`.
3. **Carry (kinematic weld):** while attached, suppress normal hit impulse (existing `_OnHit`
   early-return). Each tick set ball pos = `car.pos + car.rotMat*localOffset`, ball vel =
   `car.vel + ω×worldOffset` (rigid-body point velocity). `engageTimer += tickTime`.
4. **Boost lock (Spike Rush = NORMAL boost, not recharge):** Spike Rush keeps soccar-style boost
   (`rechargeBoostEnabled = false`; pads active when free). While a car IS the carrier, force its
   `boost = 0` and skip `BoostPad::_CheckCollide` for it (no pad pickup while attached). When free,
   normal pad pickups apply. Gate in [`Arena::Step`](src/Sim/Arena/Arena.cpp:720) using
   `ball->_internalState.attachInfo.attachedCarId`. (Gridiron differs: it uses dropshot-style recharge.)
5. **Release (powerup):** when the carrier's `controls.powerup` is true and `engageTimer >=
   MIN_ATTACH_TIME`, detach: keep current pos, set ball vel to the current point-velocity
   (`car.vel + ω×offset`) so a flip-release whips the puck; `attachedCarId = 0`;
   `releaseCooldown = RELEASE_COOLDOWN` (2s no re-engage). `lastCarrierId` stays set.
6. **Steal = instant demo:** in [`_BtCallback_OnCarCarCollision`](src/Sim/Arena/Arena.cpp:323), in
   Spike Rush, if one car is the carrier and another car contacts it, the **carrier is demoed
   regardless of speed** (`carrier->Demolish(respawnDelay)`), and the ball re-attaches to the
   toucher at the new contact point (`attachedCarId = toucher.id`, `lastCarrierId = toucher.id`).
   (Team demos: allow steal-by-teammate too — Spike Rush steals are not team-gated.)
7. **Scoring:** soccar goal plane (already wired). `IsBallScored`/`IsBallProbablyGoingIn` keep the
   soccar branch. rlgym reads `lastCarrierId` at the goal tick to attribute the goal.

## Gridiron mechanics (soccar mesh, 4v4, football)

Same files; branch on `gameMode == GRIDIRON`:

1. **Ball shape:** in [`MakeBallCollisionShape`](src/Sim/Ball/Ball.cpp:53), build a prolate-spheroid
   (capsule) convex-hull for GRIDIRON instead of a sphere, so it tumbles like a football.
2. **Roof-attach (not contact point):** on engage, `localOffset = Gridiron::ROOF_OFFSET` (fixed,
   slightly above the roof), NOT the contact point.
3. **No release button.** Release only via:
   - **Double-jump fumble:** carrier `hasDoubleJumped` → free the ball (keeps current velocity).
     Single jump is fine (no fumble).
   - **Flip lob:** carrier `isFlipping`/`hasFlipped` with a forward/back flip → release with a
     "lob": toss slightly up + along the flip's forward/back axis, spin about the axis perpendicular
     to travel, plus the carrier's forward velocity.
   - **Diagonal flip:** still throws straight forward, but the spin axis is set relative to the flip
     direction (front/back tilt), not the car's raw forward.
   - **Sideways dodge (spiral):** release with less up / more forward velocity, spinning about the
     travel axis (right dodge → clockwise spin as it flies up at a shallow angle + forward force).
   Use `car.flipRelTorque` + `car.rotMat` to derive the throw direction/spin.
4. **Steal (no demo):** if ANY other player (teammate OR opponent) touches the carrier or the
   attached ball, the ball transfers to them (`attachedCarId = toucher.id`, `lastCarrierId =
   toucher.id`) at their roof — UNLESS the carrier is within `INVULN_TIME` (0.5s) of gaining
   possession (brief invulnerability, no steal).
5. **Fumble on opponent bump:** if the carrier is bumped by an **opponent** (not a teammate) — i.e.
   a car-car contact that is NOT a steal-eligible clean touch — free the ball (fumble).
6. **Wall-ride fumble:** if the carrier is driving on a wall with wheels in contact
   (`isOnGround` via side wall, `worldContact` normal horizontal) AND its height is above the line
   (~one car length above the goal top, `WALL_FUMBLE_Z`), fumble. Airborne (no wheel contact) does
   NOT count.
7. **Re-acquire cooldown:** after losing possession, that car cannot regain for `REACQUIRE_COOLDOWN`
   (2s). Track per-car via a small timer (reuse `releaseCooldown` semantics keyed to carrier id, or
   a per-car field).
8. **Boost recharge when NOT in possession:** reuse dropshot recharge (`rechargeBoostEnabled = true`
   in the Gridiron `MutatorConfig`). The carrier's boost is locked to 0 (same gate as Spike Rush).
9. **Kickoff / possession handoff:** first kickoff → ball at field center (free). After a team
   scores, the NEXT kickoff attaches the ball to one of the **opposing team's** cars (the team that
   was scored on gets the ball). Implement in `ResetToRandomKickoff`: pick a car on the conceding
   team, set `attachedCarId`/`lastCarrierId` to it with `localOffset = ROOF_OFFSET`.
10. **Spawn:** 4v4. Add `CAR_SPAWN_LOCATIONS_GRIDIRON`/`CAR_RESPAWN_LOCATIONS_GRIDIRON` — cars lined
    up evenly in front of the goal, ~1–2 car-lengths either side of the goal center. Wire into the
    spawn-table selection in [`ResetToRandomKickoff`](src/Sim/Arena/Arena.cpp:137).
11. **Scoring:** soccar goal plane; rlgym weights 7 (carried) / 3 (released) / 3 (own). Core just
    fires the team-only goal callback.

## Shared / cross-cutting

- **`_OnHit` early-return** stays for both modes (weld suppresses normal impulse) — but Spike Rush
  must NOT early-return before the 2s arming delay (pre-arm hits are normal soccar hits).
- **Serialization:** `attachInfo` (incl. `lastCarrierId`) round-trips via `BALLSTATE_SERIALIZATION_FIELDS`.
  `CarControls.powerup` round-trips via `CAR_CONTROLS_SERIALIZATION_FIELDS`. Bump `RS_VERSION`
  ([`Framework.h`](src/Sim/Framework.h:3)) to `2.2.3` (both `BallState` and `CarControls` changed).
- **Clone/Deserialize:** ensure `attachInfo` is carried through `Arena::Clone` (it copies via
  `ball->SetState(GetState())`, so it flows automatically) and `DeserializeNew`.

## Verification (C++ side)

- Spike Rush: before 2s, ball bounces normally; after 2s, touch attaches at contact point; flipping
  the carrier keeps the puck glued to the roof (rotating offset); powerup releases keeping momentum;
  a spinning carrier whips the puck on release; touching the carrier demos them and transfers the
  ball; carrier boost stays 0 and pads don't refill it.
- Gridiron: roof-attach (not contact point); single jump keeps ball, double jump fumbles; forward
  flip lobs, right dodge spirals (clockwise); teammate/opponent touch steals (unless within 0.5s
  invuln); opponent bump fumbles; wall-ride above the line fumbles; 2s re-acquire lockout; kickoff
  after a goal attaches to the conceding team's car; football shape tumbles.
- Serialize mid-carry → deserialize → `attachedCarId`/`lastCarrierId`/offset survive.
- Soccar/hoops/heatseeker/snowday byte-identical to before.

## Out of scope (other tracks)

- rlgym router + obs + 7/3/own-goal scoring + first-to-50 (rlgym track; see T16).
- Bindings exposure of `powerup`/`attach_info`/`last_carrier_id`/football shape (bindings track).
