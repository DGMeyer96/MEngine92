#include "player.h"
#include "helpful_math.h"

static int map[10][10];

void CreatePlayer(Vector2 init_position, float init_rotation, float move_speed, float rotate_speed, float collider_radius, unsigned int map_data[10][10])
{
	player.position = init_position;
	player.rotation = init_rotation;
	player.forward = Vector2Forward(init_rotation);
	player.move_speed = move_speed;
	player.rotate_speed = rotate_speed;
	player.collider_radius = collider_radius;
	UpdatePlayerMapData(map_data);
}

void UpdatePlayerMapData(unsigned int map_data[10][10])
{
	// Copy over each value from new map data
	for (int row = 0; row < 10; row++)
	{
		for (int col = 0; col < 10; col++)
		{
			map[row][col] = map_data[row][col];
		}
	}
}

/*
 * Handles all input from keyboard.  WASD and arrow keys are used for movement and rotation.
 */
void PlayerInput()
{
	// Turn Left
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
	{
		player.rotation -= player.rotate_speed * GetFrameTime();
		player.forward = Vector2Forward(player.rotation);
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}
	// Turn Right
	else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
	{
		player.rotation += player.rotate_speed * GetFrameTime();
		player.forward = Vector2Forward(player.rotation);
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}
	// Move Forward
	if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
	{
		Vector2 new_position = Vector2Add(
			Vector2Scale(
				player.forward,
				player.move_speed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(new_position)) { player.position = new_position; }
	}
	// Move Backward
	else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
	{
		Vector2 new_position = Vector2Add(
			Vector2Scale(
				player.forward,
				-player.move_speed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(new_position)) { player.position = new_position; }
	}
}

/*
 * Check if the Player can move to the new position. Called when trying to mvoe forward or backward.
 * Checks if the spaces in the cardinal directions (up, down, left, right) contain a wall. If
 * they do contain a wall, an AABB collision check is made. If no collisions are found between any
 * of the walls, returns true meaning the player can move to the new position.
 */
bool CanMove(Vector2 position)
{
	bool hitDown = false;
	bool hitLeft = false;
	bool hitRight = false;
	bool hitUp = false;
	Rectangle wall;

	if (map[(int)position.y][(int)position.x + 1] == 1)
	{
		wall = (Rectangle){ (int)position.x + 1, (int)position.y, 1.0f, 1.0f };
		hitRight = CheckCollisionCircleRec(position, player.collider_radius, wall);
	}
	if (map[(int)position.y][(int)position.x - 1] == 1)
	{
		wall = (Rectangle){ (int)position.x - 1, (int)position.y, 1.0f, 1.0f };
		hitLeft = CheckCollisionCircleRec(position, player.collider_radius, wall);
	}
	if (map[(int)position.y + 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y + 1, 1.0f, 1.0f };
		hitDown = CheckCollisionCircleRec(position, player.collider_radius, wall);
	}
	if (map[(int)position.y - 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y - 1, 1.0f, 1.0f };
		hitUp = CheckCollisionCircleRec(position, player.collider_radius, wall);
	}

	return !hitDown && !hitLeft && !hitRight && !hitUp;
}