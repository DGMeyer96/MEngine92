#pragma once

#include "raylib.h"

typedef struct Player {
	Vector2 position;
	float rotation;
	float move_speed;
	float rotate_speed;
	float collider_radius;
	Vector2 forward;
} Player;

Player player;

void CreatePlayer(Vector2 init_position, float init_rotation, float move_speed, float rotate_speed, float collider_radius, unsigned int map_data[10][10]);
void UpdatePlayerMapData(unsigned int map_data[10][10]);
void PlayerInput();
bool CanMove(Vector2 position);