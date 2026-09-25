#include "Ball.h"

#include "../../RLConst.h"
#include "../Car/Car.h"

#include "../../../libsrc/bullet3-3.24/BulletDynamics/Dynamics/btDynamicsWorld.h"
#include "../../../libsrc/bullet3-3.24/BulletCollision/CollisionShapes/btConvexHullShape.h"
#include "../CollisionMasks.h"

RS_NS_START

bool BallState::Matches(const BallState& other, float marginPos, float marginVel, float marginAngVel) const {
	return
		pos.DistSq(other.pos) < (marginPos * marginPos) &&
		vel.DistSq(other.vel) < (marginVel * marginVel) &&
		angVel.DistSq(other.angVel) < (marginAngVel * marginAngVel);
}

void BallState::Serialize(DataStreamOut& out) {
	out.WriteMultiple(BALLSTATE_SERIALIZATION_FIELDS);
}

void BallState::Deserialize(DataStreamIn& in) {
	in.ReadMultiple(BALLSTATE_SERIALIZATION_FIELDS);
}

BallState Ball::GetState() {
	_internalState.pos = _rigidBody.getWorldTransform().getOrigin() * BT_TO_UU;
	_internalState.rotMat = _rigidBody.getWorldTransform().getBasis();
	_internalState.vel = _rigidBody.getLinearVelocity() * BT_TO_UU;
	_internalState.angVel = _rigidBody.getAngularVelocity();
	return _internalState;
}

void Ball::SetState(const BallState& state) {

	_internalState = state;

	btTransform newTransform;
	newTransform.setOrigin(state.pos * UU_TO_BT);
	newTransform.setBasis(state.rotMat);
	_rigidBody.setWorldTransform(newTransform);
	_rigidBody.setLinearVelocity(state.vel * UU_TO_BT);
	_rigidBody.setAngularVelocity(state.angVel);
	_rigidBody.updateInertiaTensor();
	if (!state.vel.IsZero() || !state.angVel.IsZero())
		_rigidBody.setActivationState(ACTIVE_TAG);

	_velocityImpulseCache = { 0,0,0 };
	_internalState.tickCountSinceUpdate = 0;
}

btCollisionShape* MakeBallCollisionShape(GameMode gameMode, const MutatorConfig& mutatorConfig, btVector3& localIntertia) {
	
	if (gameMode == GameMode::GRIDIRON) {
		using namespace RLConst;

		// Prolate-spheroid ("football") convex hull: tessellate rings of points along the local X axis.
		auto shape = new btConvexHullShape();

		int rings = (int)Gridiron::FOOTBALL_HULL_RINGS;
		int ringPts = (int)Gridiron::FOOTBALL_HULL_RING_POINTS;
		float semiMajor = Gridiron::FOOTBALL_SEMI_MAJOR * UU_TO_BT;
		float semiMinor = Gridiron::FOOTBALL_SEMI_MINOR * UU_TO_BT;

		for (int i = 0; i <= rings; i++) {
			// Parametrize the major axis from -semiMajor..+semiMajor
			float tx = (float)i / (float)rings; // 0..1
			float x = (tx * 2.f - 1.f) * semiMajor;
			// Cross-section radius at this x: ellipse cross-section
			float cross = 1.f - (x / semiMajor) * (x / semiMajor);
			float r = semiMinor * sqrtf(RS_MAX(0.f, cross));
			for (int j = 0; j < ringPts; j++) {
				float ang = (M_PI * 2.f) * ((float)j / (float)ringPts);
				Vec point = Vec(x, cosf(ang) * r, sinf(ang) * r);
				shape->addPoint(point, false);
			}
		}
		shape->recalcLocalAabb();
		shape->calculateLocalInertia(mutatorConfig.ballMass, localIntertia);
		return shape;
	} else if (gameMode == GameMode::SNOWDAY) {
		using namespace RLConst;

		auto shape = new btConvexHullShape();
		
		float angStep = (M_PI * 2) / Snowday::PUCK_CIRCLE_POINT_AMOUNT;
		float curAng = 0;
		for (int i = 0; i < Snowday::PUCK_CIRCLE_POINT_AMOUNT; i++) {
			Vec point = Vec(
				cosf(curAng) * mutatorConfig.ballRadius * UU_TO_BT,
				sinf(curAng) * mutatorConfig.ballRadius * UU_TO_BT,
				Snowday::PUCK_HEIGHT / 2 * UU_TO_BT
			);

			shape->addPoint(point, false);
			point.z *= -1;
			shape->addPoint(point, true);

			curAng += angStep;
		}
		shape->recalcLocalAabb();
		shape->calculateLocalInertia(mutatorConfig.ballMass, localIntertia);
		return shape;
	} else {
		auto shape = new btSphereShape(mutatorConfig.ballRadius * UU_TO_BT);
		shape->calculateLocalInertia(mutatorConfig.ballMass, localIntertia);
		return shape;
	}
}

void Ball::_BulletSetup(GameMode gameMode, btDynamicsWorld* bulletWorld, const MutatorConfig& mutatorConfig, bool noRot) {
	btVector3 localIneria;
	_collisionShape = MakeBallCollisionShape(gameMode, mutatorConfig, localIneria);

	btRigidBody::btRigidBodyConstructionInfo constructionInfo =
		btRigidBody::btRigidBodyConstructionInfo(mutatorConfig.ballMass, NULL, _collisionShape);

	constructionInfo.m_startWorldTransform.setIdentity();
	constructionInfo.m_startWorldTransform.setOrigin(btVector3(0, 0, mutatorConfig.ballRadius * UU_TO_BT));

	constructionInfo.m_localInertia = localIneria;
	constructionInfo.m_linearDamping = mutatorConfig.ballDrag;
	constructionInfo.m_friction = mutatorConfig.ballWorldFriction;
	constructionInfo.m_restitution = mutatorConfig.ballWorldRestitution;

	_rigidBody = btRigidBody(constructionInfo);
	_rigidBody.setUserIndex(BT_USERINFO_TYPE_BALL);
	_rigidBody.setUserPointer(this);

	// Trigger the Arena::_BulletContactAddedCallback() when anything touches the ball
	_rigidBody.m_collisionFlags |= btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK;

	_rigidBody.m_rigidbodyFlags = 0;

	_rigidBody.m_noRot = noRot && (_collisionShape->getShapeType() == SPHERE_SHAPE_PROXYTYPE);

	int mask = btBroadphaseProxy::AllFilter;
	if (!mutatorConfig.enableCarBallCollision)
		mask &= ~btBroadphaseProxy::CharacterFilter;
	bulletWorld->addRigidBody(&_rigidBody, btBroadphaseProxy::DefaultFilter | CollisionMasks::HOOPS_NET | CollisionMasks::DROPSHOT_TILE, mask);
}

void Ball::_FinishPhysicsTick(const MutatorConfig& mutatorConfig) {
	using namespace RLConst;

	// Add velocity cache
	if (!_velocityImpulseCache.IsZero()) {
		_rigidBody.m_linearVelocity += _velocityImpulseCache;
		_velocityImpulseCache = { 0,0,0 };
	}

	{ // Limit velocities
		btVector3
			vel = _rigidBody.m_linearVelocity,
			angVel = _rigidBody.m_angularVelocity;

		float ballMaxSpeedBT = mutatorConfig.ballMaxSpeed * UU_TO_BT;
		if (vel.length2() > ballMaxSpeedBT * ballMaxSpeedBT)
			vel = vel.normalized() * ballMaxSpeedBT;

		if (angVel.length2() > (BALL_MAX_ANG_SPEED * BALL_MAX_ANG_SPEED))
			angVel = angVel.normalized() * BALL_MAX_ANG_SPEED;

		_rigidBody.m_linearVelocity = vel;
		_rigidBody.m_angularVelocity = angVel;
	}

	_internalState.tickCountSinceUpdate++;
}

bool Ball::IsSphere() const {
	return dynamic_cast<btSphereShape*>(_collisionShape);
}

float Ball::GetRadiusBullet() const {
	if (IsSphere()) {
		return ((btSphereShape*)_collisionShape)->getRadius();
	} else {
		return 0;
	}
}

float Ball::GetMass() const {
	return _rigidBody.getMass();
}

void Ball::_PreTickUpdate(GameMode gameMode, float tickTime, const std::unordered_set<class Car*>& cars) {
	if (gameMode == GameMode::HEATSEEKER) {
		using namespace RLConst;

		auto state = GetState();

		float yTargetDir = _internalState.hsInfo.yTargetDir;
		if (yTargetDir != 0) {
			Angle velAngle = Angle::FromVec(state.vel);

			// Determine angle to goal
			Vec goalTargetPos = Vec(0, Heatseeker::TARGET_Y * yTargetDir, Heatseeker::TARGET_Z);
			Angle angleToGoal = Angle::FromVec(goalTargetPos - state.pos);

			// Find difference between target angle and current angle
			Angle deltaAngle = angleToGoal - velAngle;
			
			// Determine speed ratio
			float curSpeed = state.vel.Length();
			float speedRatio = curSpeed / Heatseeker::MAX_SPEED;

			// Interpolate delta
			Angle newAngle = velAngle;
			float baseInterpFactor = speedRatio * tickTime;
			newAngle.yaw += deltaAngle.yaw * baseInterpFactor * Heatseeker::HORIZONTAL_BLEND;
			newAngle.pitch += deltaAngle.pitch * baseInterpFactor * Heatseeker::VERTICAL_BLEND;
			newAngle.NormalizeFix();

			// Limit pitch
			newAngle.pitch = RS_CLAMP(newAngle.pitch, -Heatseeker::MAX_TURN_PITCH, Heatseeker::MAX_TURN_PITCH);

			// Apply aggressive UE3 rotator rounding
			// (This is suprisingly important for accuracy)
			newAngle = Math::RoundAngleUE3(newAngle);
			
			// Determine new interpolated speed
			float newSpeed = curSpeed + ((state.hsInfo.curTargetSpeed - curSpeed) * Heatseeker::SPEED_BLEND);

			// Update velocity
			Vec newDir = newAngle.GetForwardVec();

			Vec newVel = newDir * newSpeed;
			_rigidBody.m_linearVelocity = newVel * UU_TO_BT;

			_internalState.hsInfo.timeSinceHit += tickTime;
		}
	} else if (gameMode == GameMode::SNOWDAY) {
		_groundStickApplied = false;
	} else if (gameMode == GameMode::DROPSHOT || gameMode == GameMode::HOOPS) {
		// Launch ball after a short delay on kickoff

		bool isDropshot = (gameMode == GameMode::DROPSHOT);

		float launchDelay = isDropshot ? RLConst::Dropshot::BALL_LAUNCH_DELAY : RLConst::BALL_HOOPS_LAUNCH_DELAY;

		float curKickoffTime = _internalState.tickCountSinceUpdate * tickTime;
		float prevKickoffTime = curKickoffTime - tickTime;

		if (prevKickoffTime < launchDelay && curKickoffTime >= launchDelay) {

			// Launch triggered
			
			// Make sure the ball is frozen at the kickoff X and Y
			BallState state = GetState();
			if (state.vel.IsZero() && state.angVel.IsZero() && state.pos.To2D().IsZero()) {

				// Apply the force
				float launchVelZ = isDropshot ? RLConst::Dropshot::BALL_LAUNCH_Z_VEL : RLConst::BALL_HOOPS_LAUNCH_Z_VEL;
				_rigidBody.applyCentralImpulse(Vec(0, 0, launchVelZ) * GetMass() * UU_TO_BT);
				_rigidBody.setActivationState(ACTIVE_TAG);
			}
		}

	} else if (gameMode == GameMode::SPIKE_RUSH || gameMode == GameMode::GRIDIRON) {
		using namespace RLConst;

		auto& info = _internalState.attachInfo;

		// Activation: GRIDIRON is always live; SPIKE_RUSH spikes arm a fixed delay after kickoff.
		// Before activation the ball behaves as a normal soccar ball (handled by _OnHit).
		if (!info.active) {
			if (gameMode == GameMode::GRIDIRON)
				info.active = true;
			else if (_internalState.tickCountSinceUpdate * tickTime >= SpikeRush::ACTIVATION_DELAY)
				info.active = true;
		}
		if (!info.active)
			return;

		// Tick down the per-car re-acquire lockout
		if (info.releaseCooldown > 0)
			info.releaseCooldown = RS_MAX(0.f, info.releaseCooldown - tickTime);

		if (info.attachedCarId != 0) {
			// Currently welded to a car: find the carrier
			Car* carrier = nullptr;
			for (Car* car : cars)
				if (car->id == info.attachedCarId) { carrier = car; break; }

			if (carrier == nullptr || carrier->_internalState.isDemoed) {
				// Carrier demolished (Spike Rush steal) or gone: free the ball. The toucher (a
				//	different car, not lastCarrierId) can grab it immediately via proximity engage.
				info.attachedCarId = 0;
				info.releaseCooldown = (gameMode == GameMode::GRIDIRON) ? Gridiron::REACQUIRE_COOLDOWN : SpikeRush::RELEASE_COOLDOWN;
			} else {
				auto carState = carrier->GetState();
				info.engageTimer += tickTime;

				// World-space offset = carrier.rotMat * localOffset (rotates with the car, even flipped)
				btMatrix3x3 carBasis = carState.rotMat;
				Vec worldOffset = (carBasis * (btVector3)info.localOffset) * BT_TO_UU;

				// Kinematic override: puck follows the carrier
				btTransform newTransform;
				newTransform.setOrigin((carState.pos + worldOffset) * UU_TO_BT);
				newTransform.setBasis(carState.rotMat);
				_rigidBody.setWorldTransform(newTransform);

				// Rigid-body point velocity: v = car.vel + ω × offset
				Vec pointVel = carState.vel + carState.angVel.Cross(worldOffset);
				_rigidBody.setLinearVelocity(pointVel * UU_TO_BT);
				_rigidBody.setActivationState(ACTIVE_TAG);

				bool release = false;
				bool throwBall = false;
				Vec throwVel = pointVel;
				Vec throwSpin = {};

				if (gameMode == GameMode::SPIKE_RUSH) {
					// Release on the rumble powerup button (after a short grace period). The puck
					//	inherits the current point-velocity, so a spinning carrier whips it.
					if (carrier->controls.powerup && info.engageTimer >= SpikeRush::MIN_ATTACH_TIME)
						release = true;
				} else { // GRIDIRON
					if (carState.hasDoubleJumped) {
						// Double-jump fumble: free the ball, keep current velocity.
						release = true;
					} else if (carState.isFlipping || carState.hasFlipped) {
						// Flip lob (forward/back) vs dodge spiral (sideways), derived from flipRelTorque.
						float fwdFlip = carState.flipRelTorque.y; // + forward flip, - back flip
						float sideFlip = carState.flipRelTorque.x; // + right dodge, - left dodge
						Vec carFwd = carState.rotMat.forward;
						Vec carRight = carState.rotMat.right;
						if (fabsf(fwdFlip) >= fabsf(sideFlip)) {
							// Flip lob: toss up + along the flip's forward/back axis, spin about the right axis.
							float dir = RS_SGN(fwdFlip);
							throwVel = carState.vel + (carFwd * (Gridiron::THROW_FLIP_LOB_FWD * dir)) + Vec(0, 0, Gridiron::THROW_FLIP_LOB_UP);
							throwSpin = carRight * (Gridiron::THROW_FLIP_LOB_SPIN * dir);
						} else {
							// Dodge spiral: less up / more forward, spin about the travel (forward) axis.
							float dir = RS_SGN(sideFlip);
							throwVel = carState.vel + (carFwd * Gridiron::THROW_DODGE_FWD) + Vec(0, 0, Gridiron::THROW_DODGE_UP);
							throwSpin = carFwd * (Gridiron::THROW_DODGE_SPIN * dir);
						}
						release = true;
						throwBall = true;
					} else if (carState.worldContact.hasContact && carState.pos.z > Gridiron::WALL_FUMBLE_Z) {
						// Wall-ride fumble: wheels on a wall above the line.
						release = true;
					}
				}

				if (release) {
					if (throwBall) {
						_rigidBody.setLinearVelocity(throwVel * UU_TO_BT);
						_rigidBody.setAngularVelocity(throwSpin);
					} else {
						_rigidBody.setLinearVelocity(pointVel * UU_TO_BT);
					}
					info.attachedCarId = 0;
					info.releaseCooldown = (gameMode == GameMode::GRIDIRON) ? Gridiron::REACQUIRE_COOLDOWN : SpikeRush::RELEASE_COOLDOWN;
				}
			}
		}
	// NOTE: When the ball is free (attachedCarId == 0), engagement happens via collision:
	//	Arena::_BtCallback_OnCarBallCollision -> Ball::_OnAttach (see Arena.cpp). No proximity logic here.
	}
}

bool Ball::_OnAttach(Car* toucher, Car* carrier, Vec worldBallPos, GameMode gameMode,
	class Arena* arena, BallTouchEventFn ballTouchEventFunc, void* ballTouchEventUserInfo) {
	using namespace RLConst;
	auto& info = _internalState.attachInfo;

	// Not yet active (e.g. Spike Rush pre-arm): behave as a normal soccar ball, no weld.
	if (!info.active)
		return false;

	if (carrier == nullptr) {
		// Free ball: engage, unless we're inside the previous carrier's re-acquire lockout.
		if (info.releaseCooldown > 0 && toucher->id == info.lastCarrierId)
			return false; // Locked out: let the normal hit impulse apply for the old carrier.
		// Engage: weld to the toucher.
		info.attachedCarId = toucher->id;
		info.lastCarrierId = toucher->id;
		if (gameMode == GameMode::GRIDIRON) {
			info.localOffset = Gridiron::ROOF_LOCAL_OFFSET;
		} else {
			// SPIKE_RUSH: weld at the contact point (ball pos relative to car, in car-local space).
			auto ts = toucher->GetState();
			info.localOffset = ts.rotMat.Dot(worldBallPos - ts.pos);
		}
		info.engageTimer = 0;
		info.releaseCooldown = 0;
		_internalState.lastHitCarID = toucher->id;
		if (ballTouchEventFunc)
			ballTouchEventFunc(arena, toucher, ballTouchEventUserInfo);
		return true;
	}

	// Ball already welded to 'carrier'.
	if (toucher->id == carrier->id)
		return true; // Carrier touched its own ball: keep weld, suppress impulse.

	// A different car touched the ball: steal, unless the carrier is still in its post-possession
	//	invulnerability window.
	if (info.engageTimer < ATTACH_INVULN_TIME)
		return true; // Carrier invulnerable: keep weld, suppress impulse.

	// Steal: transfer possession to the toucher.
	if (gameMode == GameMode::SPIKE_RUSH && toucher->team != carrier->team)
		carrier->Demolish(); // Opponent steal demos the carrier (teammate steal: no demo).

	info.attachedCarId = toucher->id;
	info.lastCarrierId = toucher->id;
	if (gameMode == GameMode::GRIDIRON) {
		info.localOffset = Gridiron::ROOF_LOCAL_OFFSET;
	} else {
		auto ts = toucher->GetState();
		info.localOffset = ts.rotMat.Dot(worldBallPos - ts.pos);
	}
	info.engageTimer = 0;
	info.releaseCooldown = 0;
	_internalState.lastHitCarID = toucher->id;
	if (ballTouchEventFunc)
		ballTouchEventFunc(arena, toucher, ballTouchEventUserInfo);
	return true;
}

void Ball::_OnHit(
	Car* car, Vec relPos,
	float& outFriction, float& outRestitution,
	GameMode gameMode, const MutatorConfig& mutatorConfig, uint64_t tickCount,
	Arena *arena,
	BallTouchEventFn ballTouchEventFunc,
	void* ballTouchEventUserInfo
) {
	using namespace RLConst;

	// Once a ball-attach mode's mechanic is live, the puck is kinematically welded to its carrier;
	//	a normal car-ball hit impulse would fight the weld, so skip the extra impulse entirely.
	//	SPIKE_RUSH behaves like normal soccar until its spikes arm (attachInfo.active becomes true).
	if ((gameMode == GameMode::SPIKE_RUSH || gameMode == GameMode::GRIDIRON) && _internalState.attachInfo.active)
		return;

	auto carState = car->GetState();
	auto ballState = GetState();

	// Override friction/restitution
	outFriction = CARBALL_COLLISION_FRICTION;
	outRestitution = CARBALL_COLLISION_RESTITUTION;

	auto& ballHitInfo = car->_internalState.ballHitInfo;

	ballHitInfo.isValid = true;

	ballHitInfo.relativePosOnBall = relPos;
	ballHitInfo.tickCountWhenHit = tickCount;

	ballHitInfo.ballPos = ballState.pos;
	ballHitInfo.extraHitVel = Vec();

	_internalState.lastHitCarID = car->id;

	if (ballTouchEventFunc)
		ballTouchEventFunc(arena, car, ballTouchEventUserInfo);

	// Once we do an extra car-ball impulse, we need to wait at least 1 tick to do it again
	if ((tickCount > ballHitInfo.tickCountWhenExtraImpulseApplied + 1) || (ballHitInfo.tickCountWhenExtraImpulseApplied > tickCount)) {
		// Apply extra hit impulse
		ballHitInfo.tickCountWhenExtraImpulseApplied = tickCount;
		Vec carForward = car->GetForwardDir();
		Vec relPos = ballState.pos - carState.pos;
		Vec relVel = ballState.vel - carState.vel;

		float relSpeed = RS_MIN(relVel.Length(), BALL_CAR_EXTRA_IMPULSE_MAXDELTAVEL_UU);

		if (relSpeed > 0) {
			bool extraZScale =
				gameMode == GameMode::HOOPS &&
				carState.isOnGround &&
				carState.rotMat.up.z > BALL_CAR_EXTRA_IMPULSE_Z_SCALE_HOOPS_NORMAL_Z_THRESH;
			float zScale = extraZScale ? BALL_CAR_EXTRA_IMPULSE_Z_SCALE_HOOPS_GROUND : BALL_CAR_EXTRA_IMPULSE_Z_SCALE;
			Vec hitDir = (relPos * Vec(1, 1, zScale)).Normalized();
			Vec forwardDirAdjustment = carForward * hitDir.Dot(carForward) * (1 - BALL_CAR_EXTRA_IMPULSE_FORWARD_SCALE);
			hitDir = (hitDir - forwardDirAdjustment).Normalized();
			Vec addedVel = (hitDir * relSpeed) * BALL_CAR_EXTRA_IMPULSE_FACTOR_CURVE.GetOutput(relSpeed) * mutatorConfig.ballHitExtraForceScale;
			ballHitInfo.extraHitVel = addedVel;

			// Velocity won't be actually added until the end of this tick
			_velocityImpulseCache += addedVel * UU_TO_BT;
		}
	} else {
		// Don't do multiple extra impulses in a row
		return;
	}

	if (gameMode == GameMode::HEATSEEKER) {
		bool canIncrease = (_internalState.hsInfo.timeSinceHit > Heatseeker::MIN_SPEEDUP_INTERVAL) || (_internalState.hsInfo.yTargetDir == 0);
		float newTargetDir = (car->team == Team::BLUE) ? 1 : -1;
		if (canIncrease && (newTargetDir != _internalState.hsInfo.yTargetDir)) {
			_internalState.hsInfo.timeSinceHit = 0;
			_internalState.hsInfo.curTargetSpeed = RS_MIN(_internalState.hsInfo.curTargetSpeed + Heatseeker::TARGET_SPEED_INCREMENT, Heatseeker::MAX_SPEED);
		}
		_internalState.hsInfo.yTargetDir = newTargetDir;
	} else if (gameMode == GameMode::DROPSHOT) {
		auto& accumulatedHitForce = _internalState.dsInfo.accumulatedHitForce;
		auto& chargeLevel = _internalState.dsInfo.chargeLevel;

		Vec dirFromCar = (ballState.pos - carState.pos).Normalized();
		Vec relVelFromCar = carState.vel - ballState.vel;
		float velIntoBall = dirFromCar.Dot(relVelFromCar);
		if (velIntoBall >= Dropshot::MIN_CHARGE_HIT_SPEED) {
			
			accumulatedHitForce += velIntoBall;

			// Normal charge
			if (accumulatedHitForce >= Dropshot::MIN_ABSORBED_FORCE_FOR_CHARGE)
				chargeLevel = 2;
			
			// Supercharge
			if (accumulatedHitForce >= Dropshot::MIN_ABSORBED_FORCE_FOR_SUPERCHARGE)
				chargeLevel = 3;
		}
		
		if (chargeLevel > 1) {
			float newTargetDir = (car->team == Team::BLUE) ? 1 : -1;
			_internalState.dsInfo.yTargetDir = newTargetDir;
		}
	}
}

void Ball::_OnWorldCollision(GameMode gameMode, Vec normal, float tickTime) {
	using namespace RLConst;

	if (gameMode == GameMode::HEATSEEKER) {
		if (_internalState.hsInfo.yTargetDir != 0 ) {
			Vec pos = _rigidBody.getWorldTransform().getOrigin() * BT_TO_UU;
			float relNormalY = normal.y * _internalState.hsInfo.yTargetDir;
			float relY = pos.y * _internalState.hsInfo.yTargetDir;
			if (relNormalY <= -Heatseeker::WALL_BOUNCE_CHANGE_Y_NORMAL && 
				relY >= ARENA_EXTENT_Y - Heatseeker::WALL_BOUNCE_CHANGE_Y_THRESH) {

				// We hit far enough to change direction
				_internalState.hsInfo.yTargetDir *= -1;

				Vec pos = _rigidBody.getWorldTransform().getOrigin() * BT_TO_UU;
				Vec vel = _rigidBody.m_linearVelocity * BT_TO_UU;

				// TODO: Make this a member function
				Vec goalTargetPos = Vec(0, Heatseeker::TARGET_Y * _internalState.hsInfo.yTargetDir, Heatseeker::TARGET_Z);

				// Add wall bounce impulse
				Vec dirToGoal = (goalTargetPos - pos).Normalized();

				Vec bounceDir =
					dirToGoal * (1 - Heatseeker::WALL_BOUNCE_UP_FRAC) +
					Vec(0, 0, 1) * Heatseeker::WALL_BOUNCE_UP_FRAC;
				Vec bounceImpulse = bounceDir * vel.Length() * Heatseeker::WALL_BOUNCE_FORCE_SCALE;
				_velocityImpulseCache += bounceImpulse * UU_TO_BT;
			}
		}
	} else if (gameMode == GameMode::SNOWDAY) {
		if (!_groundStickApplied) {
			_rigidBody.applyCentralForce(-normal * Snowday::PUCK_GROUND_STICK_FORCE);
			_groundStickApplied = true;
		}
	}
}

bool Ball::_OnDropshotTileCollision(
	DropshotTilesState& tilesState, int tileTotalIndex, const btCollisionObject* tileObj,
	uint64_t tickCount, float tickTime
) {
	int teamIdx = tileTotalIndex / RLConst::Dropshot::NUM_TILES_PER_TEAM;
	int tileIdx = tileTotalIndex % RLConst::Dropshot::NUM_TILES_PER_TEAM;
	auto& tileState = tilesState.states[teamIdx][tileIdx];
	Vec tilePos = DropshotTiles::GetTilePos(teamIdx, tileIdx);
	auto& dsInfo = _internalState.dsInfo;

	// This should be possible in rare circumstances where two tiles are hit simultaneously
	if (tileState.damageState == DropshotTileState::STATE_BROKEN)
		return false;

	if (dsInfo.hasDamaged) {
		float timeSinceDamage = (tickCount - dsInfo.lastDamageTick) * tickTime;
		if (timeSinceDamage <= RLConst::Dropshot::MIN_DAMAGE_INTERVAL)
			return false; // Hasn't been long enough since we last damaged
	}

	Vec vel = _rigidBody.getLinearVelocity() * BT_TO_UU;
	if (vel.z > -RLConst::Dropshot::MIN_DOWNWARD_SPEED_TO_DAMAGE)
		return false;

	if (dsInfo.chargeLevel > 1 && dsInfo.yTargetDir != 0)
		if (RS_SGN(tilePos.y) != dsInfo.yTargetDir)
			return false; // Wrong side of the arena

	// All checks passed

	// Break the tile(s)
	{
		std::vector<int> indicesToBreak = DropshotTiles::GetNeighborIndices(tileIdx, dsInfo.chargeLevel);
		for (int i : indicesToBreak) {
			DropshotTileState& state = tilesState.states[teamIdx][i];
			if (state.damageState != DropshotTileState::STATE_BROKEN)
				state.damageState++;
		}
	}
	dsInfo.hasDamaged = true;
	dsInfo.lastDamageTick = tickCount;
	dsInfo.accumulatedHitForce = 0;
	dsInfo.chargeLevel = 1;
	dsInfo.yTargetDir = 0;
	return true;
}

RS_NS_END
