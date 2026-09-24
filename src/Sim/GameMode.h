#pragma once
#include "../Framework.h"

RS_NS_START

enum class GameMode : byte {
	SOCCAR,
	HOOPS,
	HEATSEEKER,
	SNOWDAY,
	DROPSHOT,

	// I will not add rumble unless I am given a large amount of money, or, alternatively, a large amount of candy corn (I love candy corn)

	// Soccar but without goals, boost pads, or the arena hull. The cars and ball will fall infinitely.
	THE_VOID,

	// NOTE: The following modes are appended AFTER THE_VOID on purpose, so that the
	//	underlying byte values of the pre-existing modes above remain byte-identical
	//	(they are serialized raw in Arena::Serialize).

	// Soccar field with a "puck" that welds to a car's roof on contact and releases on jump.
	SPIKE_RUSH,
	// 4v4 soccar-field variant of the ball-attach mechanic.
	GRIDIRON,
};

constexpr const char* GAMEMODE_STRS[] = {
	"soccar",
	"hoops",
	"heatseeker",
	"snowday",
	"dropshot",
	"void",
	"spike_rush",
	"gridiron"
};

RS_NS_END