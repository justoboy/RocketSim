#pragma once
#include "../../../RLConst.h"
#include "../../../../libsrc/bullet3-3.24/BulletDynamics/Dynamics/btRigidBody.h"

RS_NS_START

struct DropshotTileState {
	enum {
		STATE_FULL = 0,
		STATE_DAMAGED,
		STATE_BROKEN
	};
	uint8_t damageState = STATE_FULL;
};

struct DropshotTilesState {
	enum {
		STATE_FULL = 0,
		STATE_DAMAGED,
		STATE_BROKEN
	};

	DropshotTileState states[RLConst::Dropshot::TEAM_AMOUNT][RLConst::Dropshot::NUM_TILES_PER_TEAM];

	DropshotTilesState() {
		for (int i = 0; i < RLConst::Dropshot::TEAM_AMOUNT; i++)
			for (int j = 0; j < RLConst::Dropshot::NUM_TILES_PER_TEAM; j++)
				states[i][j] = DropshotTileState();
	}

	// TODO: Add serialization
};

namespace DropshotTiles {
	void Init();

	Vec GetTilePos(int team, int index);
	std::vector<btCollisionShape*> MakeTileShapes();
	std::vector<int> GetNeighborIndices(int startIdx, int radius);

	// Finds the tile whose center is nearest to worldPos (in the XY ground plane),
	//	writing the owning team index (0 = BLUE / -Y, 1 = ORANGE / +Y) to *teamOut.
	//	Returns the canonical tile index [0, NUM_TILES_PER_TEAM), or -1 if none found.
	int GetTileIndexAt(Vec worldPos, int* teamOut);
};

RS_NS_END