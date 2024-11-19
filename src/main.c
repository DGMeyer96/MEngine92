/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

For a C++ project simply rename the file to .cpp and re-run the build script 

-- Copyright (c) 2020-2024 Jeffery Myers
--
--This software is provided "as-is", without any express or implied warranty. In no event 
--will the authors be held liable for any damages arising from the use of this software.

--Permission is granted to anyone to use this software for any purpose, including commercial 
--applications, and to alter it and redistribute it freely, subject to the following restrictions:

--  1. The origin of this software must not be misrepresented; you must not claim that you 
--  wrote the original software. If you use this software in a product, an acknowledgment 
--  in the product documentation would be appreciated but is not required.
--
--  2. Altered source versions must be plainly marked as such, and must not be misrepresented
--  as being the original software.
--
--  3. This notice may not be removed or altered from any source distribution.

*/

#include "raylib.h"
#include "rlgl.h"
#include "main.h" 

#include "resource_dir.h"			// utility header for SearchAndSetResourceDir
#include <stdio.h>                  // Required for: fopen(), fclose(), fputc(), fwrite(), printf(), fprintf(), funopen()
#include <time.h>                   // Required for: time_t, tm, time(), localtime(), strftime()
#include <math.h>					// Need Math extensions

// Rendering stuff
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960
#define MIN_SCREEN_WIDTH 640
#define MIN_SCREEN_HEIGHT 480
#define VIEWPORT_WIDTH 640
#define VIEWPORT_HEIGHT 480
#define FOV 70
#define HALF_FOV (FOV / 2)
#define VERTICAL_FOV (2 * atanf(tanf(FOV / 2) * (VIEWPORT_HEIGHT / VIEWPORT_WIDTH)))
#define DRAW_DISTANCE 20
#define MAP_LENGTH 10
#define TILE_SIZE_PIXELS VIEWPORT_HEIGHT / MAP_LENGTH
// FISHEYE Correction stuff
#define PROJECTION_PLANE_WIDTH (DRAW_DISTANCE * tanf(DEG2RAD * (FOV / 2)) * 2)
#define PROJECTION_PLANE_HALF_WIDTH (PROJECTION_PLANE_WIDTH / 2)
#define X_MAX (VIEWPORT_WIDTH - 1)
#define HEIGHT_RATIO ((float)VIEWPORT_HEIGHT / (float)VIEWPORT_WIDTH) / (FOV / 90.0f)
// Aspect Ratio scaling stuff
#define PROJECTION_PLANE_HEIGHT (DRAW_DISTANCE * tanf(VERTICAL_FOV / 2))
#define HALF_WALL_HEIGHT (10 / 2)

enum DrawMode drawMode = GAME;
enum ShadingMdoe shadingMode = TEXTURED;
enum Resolution currentResolution = ULTRA;

float scale = 1.0f;
int column_pixel_width = 1;
int ray_count = VIEWPORT_WIDTH;
// Auto fill with largest amount, can't resize smaller in C without too much dynamic allocation overhead for array this small.
// Basically fill it with the width of the game viewport and add 1
struct MyRay rays[VIEWPORT_WIDTH + 1];	

const int map[MAP_LENGTH][MAP_LENGTH] = {
	{ 1,1,1,1,1,1,1,1,1,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,0,0,0,0,1,0,0,1,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,1,1,0,0,0,0,0,0,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,0,1,0,0,1,1,1,0,1 },
	{ 1,0,1,0,0,1,1,1,0,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,1,1,1,1,1,1,1,1,1, },
};

struct Player player = {
	.position.x = 1.5f,
	.position.y = 1.5f,
	.rotation = 0.0f,
	.moveSpeed = 2.0f,
	.rotateSpeed = 90.0f,
	.colliderRadius = 0.2f,
};

void fillRays(struct MyRay rays[], const int length, Vector2 startPosition, float startAngle)
{
	const float rayDistance = 50.0f;
	for (int i = 0; i < length; i++)
	{
		rays[i].start = startPosition;
		rays[i].end = Vector2RotateAroundPoint(
			startPosition, 
			Vector2Add(startPosition, (Vector2) { rayDistance, 0.0f }), 
			(i - (length / 2)) + startAngle
		);
	}
}

/*
 * Standard DDA algorithm that uses fixed angle step for casting each ray. As a result, this does
 * produce the "fisheye" distortion that can be corrected through cos().  However, this distortion
 * can only be corrected for an FOV of around 75 degrees.  Anything above this you start to see
 * a reverse fisheye distortion around the edges of the screen.
 */
void DDA(struct MyRay rays[], Vector2 position, float angle)
{	
	const float angleStep = (float)FOV / (float)ray_count;

	for (int i = 0; i <= ray_count; i++)
	{
		rays[i].start = position;

		// This is slope (m = dx / dy), dx = cos(angle), dy = sin(angle)
		// First step is -45
		// Last step is 45
		// Middle step is 0
		// (i * step)
		float rayAngle = (i * angleStep) + angle - HALF_FOV;
		Vector2 forward = Vector2Forward(position, rayAngle);

		// Convert pixel coords into map grid coords
		int mapCol = position.x;
		int mapRow = position.y;

		// With slope we need to calculate the how far 1 step in either direction is
		// i.e. 
		// if dx = 1 then dy / dx = 1
		// By pythagoras dx/dy = sqrt[1^2 + (dy/dx)^2]
		// 1 unit in x = sqrt[1^2 + (dy/dx)^2]
		// 1 unit in y = sqrt[1^2 + (dx/dy)^2]
		Vector2 step = (Vector2){
			sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
			sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
		};
		Vector2 rayLength = Vector2Zero();
		int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
		// Get length of ray to move 1 step along X-Axis
		if (forward.x < 0)
		{
			dirX = -1;
			rayLength.x = (position.x - (float)(mapCol)) * step.x;
		}
		else
		{
			dirX = 1;
			rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
		}
		// Get length of ray to move 1 step along Y-Axis
		if (forward.y < 0)
		{
			dirY = -1;
			rayLength.y = (position.y - (float)(mapRow)) * step.y;
		}
		else
		{
			dirY = 1;
			rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
		}

		bool hitWall = false;
		bool hitX = false;
		float distanceChecked = 0.0f;
		while (!hitWall && distanceChecked < DRAW_DISTANCE)
		{
			// Step along shortest length
			if (rayLength.x < rayLength.y)
			{
				mapCol += dirX;
				distanceChecked = rayLength.x;
				rayLength.x += step.x;
				hitX = true;
			}
			else
			{
				mapRow += dirY;
				distanceChecked = rayLength.y;
				rayLength.y += step.y;
				hitX = false;
			}

			hitWall = map[mapRow][mapCol] == 1;
		}

		// Save for rendering shadowed walls
		rays[i].hitX = hitX;
		if (hitX)
		{
			rays[i].distance = rayLength.x - step.x;
		}
		else
		{
			rays[i].distance = rayLength.y - step.y;
		}

		rays[i].end = Vector2Add(position, Vector2Scale(forward, distanceChecked));

		// Draw Ray and collision point
		//DrawLine(rays[i].start.x * TILE_SIZE_PIXELS, rays[i].start.y * TILE_SIZE_PIXELS, rays[i].end.x * TILE_SIZE_PIXELS, rays[i].end.y * TILE_SIZE_PIXELS, PURPLE);
		//DrawCircle(rays[i].end.x * TILE_SIZE_PIXELS, rays[i].end.y * TILE_SIZE_PIXELS, 2.0f, PURPLE);
	}
}

/*
 * DEPRECATED
 * Mostly used for debugging. Fires a single ray ddirectly if front of the user and draws each 
 * step until it hits the end.
 */
void DDASingle(Vector2 position, float angle)
{
	struct MyRay ray;
	ray.start = position;

	Vector2 forward = Vector2Forward(position, angle);

	// Convert pixel coords into map grid coords
	int mapCol = position.x;
	int mapRow = position.y;

	// With slope we need to calculate the how far 1 step in either direction is
	// i.e. 
	// if dx = 1 then dy / dx = 1
	// By pythagoras dx/dy = sqrt[1^2 + (dy/dx)^2]
	// 1 unit in x = sqrt[1^2 + (dy/dx)^2]
	// 1 unit in y = sqrt[1^2 + (dx/dy)^2]
	Vector2 step = (Vector2){
		sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
		sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
	};

	Vector2 rayLength = Vector2Zero();
	int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
	// Get length of ray to move 1 step along X-Axis
	if (forward.x < 0) 
	{ 
		dirX = -1;
		rayLength.x = (position.x - (float)(mapCol)) * step.x;
	}
	else
	{
		dirX = 1;
		rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
	}
	// Get length of ray to move 1 step along Y-Axis
	if (forward.y < 0) 
	{ 
		dirY = -1; 
		rayLength.y = (position.y - (float)(mapRow)) * step.y;
	}
	else
	{
		dirY = 1;
		rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
	}
	//TraceLog(LOG_INFO, "rayLength = %f | %f", rayLength.x, rayLength.y);
	
	bool hitWall = false;
	const float maxDistance = 500.0f;	// Draw Distance
	float distanceChecked = 0.0f;
	//TraceLog(LOG_INFO, "");
	while (!hitWall && distanceChecked < maxDistance)
	{
		// Step along shortest length
		if (rayLength.x < rayLength.y)
		{
			mapCol += dirX;
			distanceChecked = rayLength.x;
			rayLength.x += step.x;
		}
		else
		{
			mapRow += dirY;
			distanceChecked = rayLength.y;
			rayLength.y += step.y;
		}
		ray.end = (Vector2){ position.x, position.y };
		ray.end = Vector2Add(ray.end, Vector2Scale(forward, distanceChecked));
		DrawCircle(ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, 5.0f, PURPLE);

		hitWall = map[mapRow][mapCol] == 1;
	}

	ray.end = (Vector2){ position.x, position.y};
	ray.end = Vector2Add(ray.end, Vector2Scale(forward, distanceChecked));

	// Draw Ray and collision point
	DrawLine(ray.start.x * TILE_SIZE_PIXELS, ray.start.y * TILE_SIZE_PIXELS, ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, PURPLE);
	DrawCircle(ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, 5.0f, PURPLE);

	DrawText(
		TextFormat("Ray Start: (%f, %f)", ray.start.x, ray.start.y),
		500, 100, 20, WHITE
	);
	DrawText(
		TextFormat("Ray End: (%f, %f)", ray.end.x, ray.end.y),
		500, 125, 20, WHITE
	);


	DrawText(TextFormat("Forward = (%f, %f)", forward.x, forward.y), 500, 175, 20, WHITE);
	DrawText(TextFormat("Direction = (%d, %d)", dirX, dirY), 500, 200, 20, WHITE);
	DrawText(TextFormat("Step = (%f, %f)", step.x, step.y), 500, 225, 20, WHITE);

	DrawText(TextFormat("End Map Coords = (%d, %d)", mapCol, mapRow), 500, 250, 20, WHITE);
	DrawText(TextFormat("Distance Checked = %f", distanceChecked), 500, 275, 20, WHITE);
	DrawText(TextFormat("Ray Length = (%f, %f)", rayLength.x, rayLength.y), 500, 300, 20, WHITE);
}

/*
 * DDA using a non-linear angle step for casting each ray. The math for calculating the angles and
 * distance can be found at https://www.scottsmitelli.com/articles/we-can-fix-your-raycaster/.
 */
void DDANonLinear(struct MyRay rays[], Vector2 position, float angle)
{
	const int xPixelWidth = VIEWPORT_WIDTH / ray_count;
	const int halfRayCount = ray_count / 2;

	// Calculate angles
	for (int i = 0; i <= halfRayCount; i++)
	{
		float xScreen = i * xPixelWidth;
		float X_PROJECTION_PLANE = (((float)(xScreen * 2) - X_MAX) / X_MAX) * (PROJECTION_PLANE_HALF_WIDTH);
		float castAngle = atan2f(X_PROJECTION_PLANE, DRAW_DISTANCE);

		rays[i].castAngleRadians = castAngle;
		rays[ray_count - i].castAngleRadians = -castAngle;
	}

	// Cast the rays
	for (int i = 0; i <= ray_count; i++)
	{
		rays[i].start = position;

		float angle = (rays[i].castAngleRadians * RAD2DEG) + player.rotation;
		Vector2 forward = Vector2Forward(position, angle);
		Vector2 step = (Vector2){
			sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
			sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
		};

		// Convert pixel coords into map grid coords
		int mapCol = position.x;
		int mapRow = position.y;

		Vector2 rayLength = Vector2Zero();
		int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
		// Get length of ray to move 1 step along X-Axis
		if (forward.x < 0)
		{
			dirX = -1;
			rayLength.x = (position.x - (float)(mapCol)) * step.x;
		}
		else
		{
			dirX = 1;
			rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
		}
		// Get length of ray to move 1 step along Y-Axis
		if (forward.y < 0)
		{
			dirY = -1;
			rayLength.y = (position.y - (float)(mapRow)) * step.y;
		}
		else
		{
			dirY = 1;
			rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
		}

		bool hitWall = false;
		bool hitX = false;
		float distanceChecked = 0.0f;
		while (!hitWall && distanceChecked < DRAW_DISTANCE)
		{
			// Step along shortest length
			if (rayLength.x < rayLength.y)
			{
				mapCol += dirX;
				distanceChecked = rayLength.x;
				rayLength.x += step.x;
				hitX = true;
			}
			else
			{
				mapRow += dirY;
				distanceChecked = rayLength.y;
				rayLength.y += step.y;
				hitX = false;
			}

			hitWall = map[mapRow][mapCol] == 1;
		}

		
		rays[i].end = Vector2Add(position, Vector2Scale(forward, distanceChecked));

		// Save for rendering shadowed walls
		rays[i].hitX = hitX;
		// Choose which distance and offset value to store based on if we hit horizontal or vertical wall
		if (hitX)	// Horizontal wall hit
		{
			rays[i].distance = (rayLength.x - step.x) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.y, 1.0f);
		}
		else       // Vertical wall hit
		{
			rays[i].distance = (rayLength.y - step.y) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.x, 1.0f);
		}
	}
}

/*
 * DDA using a non-linear angle step for casting each ray. The math for calculating the angles and
 * distance can be found at https://www.scottsmitelli.com/articles/we-can-fix-your-raycaster/.
 */
void DrawDebug()
{
	// FPS & Frametime
	DrawText(TextFormat("FPS: %d", (int)(1 / GetFrameTime())), 0, 0, 20, WHITE);
	DrawText(TextFormat("Frametime: %.2fms", GetFrameTime() * 1000.0f), 0, 20, 20, WHITE);
	DrawText(TextFormat("Draw Mode: %d", drawMode), 0, 40, 20, WHITE);
	DrawText(TextFormat("Scale: %f", scale), 0, 60, 20, WHITE);
	DrawText(TextFormat("Screen: ( %d , %d )", GetScreenWidth(), GetScreenHeight()), 0, 80, 20, WHITE);
	//DrawText(TextFormat("Render: ( %d , %d )", GetRenderWidth(), GetRenderHeight()), 0, 140, 20, WHITE);
	//DrawText(TextFormat("Player Position: ( %f , %f )", player.position.x, player.position.y), 0, 40, 20, WHITE);
	//DrawText(TextFormat("Player Rotation: %f", player.rotation), 0, 60, 20, WHITE);
	//DrawText(TextFormat("Player Forward: ( %f , %f )", forward.x, forward.y), 0, 80, 20, WHITE);
}

/*
 * Draws the 2D version of the map. Usefule as a type of "automap" and useful for debugging.
 */
void Draw2D(const struct MyRay rays[])
{
	// Draw Map
	for (int row = 0; row < MAP_LENGTH; row++)
	{
		for (int col = 0; col < MAP_LENGTH; col++)
		{
			if (map[row][col] == 1)
			{
				// Walls
				DrawRectangle(TILE_SIZE_PIXELS * col, TILE_SIZE_PIXELS * row, TILE_SIZE_PIXELS - 2, TILE_SIZE_PIXELS - 2, RED);
			}
			else
			{
				// Open Space
				DrawRectangle(TILE_SIZE_PIXELS * col, TILE_SIZE_PIXELS * row, TILE_SIZE_PIXELS - 2, TILE_SIZE_PIXELS - 2, BLUE);
			}
		}
	}

	for (int i = 0; i <= ray_count; i++)
	{
		if (i >= (ray_count / 2) - 3 && i <= (ray_count / 2) + 3)
		{
			DrawLine(
				rays[i].start.x * TILE_SIZE_PIXELS,
				rays[i].start.y * TILE_SIZE_PIXELS,
				rays[i].end.x * TILE_SIZE_PIXELS,
				rays[i].end.y * TILE_SIZE_PIXELS,
				YELLOW
			);
		}
		else {
			DrawLine(
				rays[i].start.x * TILE_SIZE_PIXELS,
				rays[i].start.y * TILE_SIZE_PIXELS,
				rays[i].end.x * TILE_SIZE_PIXELS,
				rays[i].end.y * TILE_SIZE_PIXELS,
				PURPLE
			);
		}
	}

	// Draw Player
	DrawCircle(player.position.x * TILE_SIZE_PIXELS, player.position.y * TILE_SIZE_PIXELS, player.colliderRadius * TILE_SIZE_PIXELS, GREEN);
	Vector2 temp = player.forward;
	temp = Vector2Scale(temp, 25.0f);
	temp = Vector2Add(temp, Vector2Scale(player.position, TILE_SIZE_PIXELS));
	DrawLine(player.position.x * TILE_SIZE_PIXELS, player.position.y * TILE_SIZE_PIXELS, temp.x, temp.y, GREEN);
}

/*
 * Draws the 3D version of the map. Takes an array of rays that have been filled by DDANonLinear().
 * Also takes in a texture to draw on the walls. Will update this later to look at map data for
 * the texture to use instead of a single texture. Draws Ceiling and Floor first.  Next goes
 * through the ray data and draws each column at a fixed width and adjsuts the height based on 
 * distance from the Player.
 */
void Draw3D(struct MyRay rays[], Texture tex)
{
	const float widthPercent = (float)column_pixel_width / (float)VIEWPORT_WIDTH;
	// Draw Ceiling
	DrawRectangle(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, LIGHTGRAY);
	// Draw Floor
	DrawRectangle(0, VIEWPORT_HEIGHT / 2, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, DARKGRAY);
	// Walls
	for (int i = 0; i <= ray_count; i++)
	{
		// Calculate the height based on distance from camera
		float height = (VIEWPORT_HEIGHT * HEIGHT_RATIO) / rays[i].distance;
		float heightPercent = 1.0f;

		float texOffset = (float)tex.height;
		float texStartOffset = 0.0f;
		/*TraceLog(LOG_INFO, "Height: %f | Radio: %f", height, height * HEIGHT_RATIO);*/
		// Clamp the height so we don't draw outside of the viewport
		if (height > VIEWPORT_HEIGHT) { 
			heightPercent = height / VIEWPORT_HEIGHT;
			 
			texStartOffset = 1.0f / heightPercent;
			texOffset *= texStartOffset;
			texStartOffset = ((1.0f - texStartOffset) / 2.0f) * tex.height;

			// Keep height from exceeding height of viewport
			height /= heightPercent;
		}
			
		Color wallColor = DARKGRAY;
		// Shade walls darker if they are perpedicular
		if (rays[i].hitX) {
			wallColor = WHITE;
		}
		// Scale for brightness, lower number reduces amount of "light" emitted by player
		const float brightnessScaler = 4.0f;
		float brightness = brightnessScaler / rays[i].distance;
		if (brightness > 1.0f) { brightness = 1.0f; }
		wallColor.r *= brightness;
		wallColor.g *= brightness;
		wallColor.b *= brightness;

		if (shadingMode == TEXTURED)
		{
			// Draw Wall (Textured)
			Rectangle texCoords = (Rectangle){
				rays[i].offset * tex.width,
				texStartOffset,
				widthPercent * tex.width,
				texOffset,
			};
			Rectangle position = (Rectangle){
				i * column_pixel_width,
				(VIEWPORT_HEIGHT / 2) - (height / 2),
				column_pixel_width,
				height,
			};
			DrawTexturePro(
				tex,
				texCoords,
				position,
				Vector2Zero(),
				0.0f,
				wallColor
			);
		}
		else if (shadingMode == FLAT)
		{
			wallColor = RED;
			// Shade walls darker if they are perpedicular
			if (!rays[i].hitX) {
				wallColor.r *= 0.5f;
				wallColor.g *= 0.5f;
				wallColor.b *= 0.5f;
			}
			// Draw Wall (Flat Shaded)
			DrawRectangle(
				i * column_pixel_width,
				(VIEWPORT_HEIGHT / 2) - (height / 2),
				column_pixel_width,
				height,
				wallColor
			);
		}
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
		wall = (Rectangle){(int)position.x + 1, (int)position.y, 1.0f, 1.0f };
		if (drawMode == MAP) {
			DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		}
		hitRight = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y][(int)position.x - 1] == 1)
	{
		wall = (Rectangle){ (int)position.x - 1, (int)position.y, 1.0f, 1.0f };
		if (drawMode == MAP) {
			DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		}
		hitLeft = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y + 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y + 1, 1.0f, 1.0f };
		if (drawMode == MAP) {
			DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		}
		hitDown = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y - 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y - 1, 1.0f, 1.0f };
		if (drawMode == MAP) {
			DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		}
		hitUp = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}

	return !hitDown && !hitLeft && !hitRight && !hitUp;
}

/*
 * Handles all input from keyboard.  WASD and arrow keys are used for movement and rotation. 
 * TAB, R, T are used for debug functions such as switching draw modes, render resolution and shading.
 */
void HandleInput()
{
	// Toggle between Auto Map and Game View
	if (IsKeyPressed(KEY_TAB))
	{
		switch (drawMode)
		{
			case GAME:
				drawMode = GAME_DEBUG;
				break;
			case GAME_DEBUG:
				drawMode = MAP;
				break;
			case MAP:
				drawMode = MAP_DEBUG;
				break;
			case MAP_DEBUG:
				drawMode = GAME;
				break;
		}
	}
	// Cycle through render resolution (ray count)
	if (IsKeyPressed(KEY_R))
	{
		switch (currentResolution)
		{
			case VERY_LOW:
				currentResolution = LOW;
				column_pixel_width = 5;
				break;
			case LOW:
				currentResolution = MEDIUM;
				column_pixel_width = 4;
				break;
			case MEDIUM:
				currentResolution = HIGH;
				column_pixel_width = 2;
				break;
			case HIGH:
				currentResolution = ULTRA;
				column_pixel_width = 1;
				break;
			case ULTRA:
				currentResolution = VERY_LOW;
				column_pixel_width = 8;
				break;

		}
		ray_count = VIEWPORT_WIDTH / column_pixel_width;
		TraceLog(LOG_INFO, "Column Pixel Width: %d | Ray Count: %d", column_pixel_width, ray_count);
	}
	// Toggle between shading modes (texture/flat)
	if (IsKeyPressed(KEY_T))
	{
		switch (shadingMode)
		{
			case TEXTURED:
				shadingMode = FLAT;
				break;
			case FLAT:
				shadingMode = TEXTURED;
				break;
		}
	}
	// Turn Left
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
	{
		player.rotation -= player.rotateSpeed * GetFrameTime();
		player.forward = Vector2Forward(player.position, player.rotation);
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}
	// Turn Right
	else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
	{
		player.rotation += player.rotateSpeed * GetFrameTime();
		player.forward = Vector2Forward(player.position, player.rotation);
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}
	// Move Forward
	if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
	{
		Vector2 newPosition = Vector2Add(
			Vector2Scale(
				player.forward,
				player.moveSpeed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(newPosition)) { player.position = newPosition; }
	}
	// Move Backward
	else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
	{
		Vector2 newPosition = Vector2Add(
			Vector2Scale(
				player.forward,
				-player.moveSpeed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(newPosition)) { player.position = newPosition; }
	}
}

int main ()
{
	SetTraceLogLevel(LOG_ALL);

	// Tell the window to use vysnc and work on high DPI displays
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_MAXIMIZED);
	// Create the window and OpenGL context
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MEngine92");
	SetWindowMinSize(MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
	// Render texture initialization, used to hold the rendering result so we can easily resize it
	RenderTexture2D target = LoadRenderTexture(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
	// Texture scale filter to use
	SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);  

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	// Load textures from the resources directory
	Texture wabbit = LoadTexture("wabbit_alpha.png");
	Texture checkerboard = LoadTexture("checkerboard.png");
	Texture checkerboard2 = LoadTexture("checkerboard2.png");
	Texture checkerboard64 = LoadTexture("checkerboard64.png");
	Texture greyBrick = LoadTexture("grey_brick_32.png");
	Texture redBrick = LoadTexture("red_brick.png");
	Texture metal = LoadTexture("metal.png");
	Texture texCoordsDebug = LoadTexture("tex_coords.png");
	
	// game loop
	while (!WindowShouldClose())		// run the loop untill the user presses ESCAPE or presses the Close button on the window
	{
		// Update
		//----------------------------------------------------------------------------------
		// Compute required framebuffer scaling
		scale = MIN((float)GetScreenWidth() / VIEWPORT_WIDTH, (float)GetScreenHeight() / VIEWPORT_HEIGHT);
		//renderScale.x = 1.0f / ((float)GetScreenWidth() / VIEWPORT_WIDTH);
		//renderScale.y = 1.0f / ((float)GetScreenHeight() / VIEWPORT_HEIGHT);

		// Update virtual mouse (clamped mouse value behind game screen)
		Vector2 mouse = GetMousePosition();
		Vector2 virtualMouse = { 0 };
		virtualMouse.x = (mouse.x - (GetScreenWidth() - (VIEWPORT_WIDTH * scale)) * 0.5f) / scale;
		virtualMouse.y = (mouse.y - (GetScreenHeight() - (VIEWPORT_HEIGHT * scale)) * 0.5f) / scale;
		virtualMouse = Vector2Clamp(virtualMouse, (Vector2) { 0, 0 }, (Vector2) { (float)VIEWPORT_WIDTH, (float)VIEWPORT_HEIGHT});

		// Apply the same transformation as the virtual mouse to the real mouse (i.e. to work with raygui)
		//SetMouseOffset(-(GetScreenWidth() - (gameScreenWidth*scale))*0.5f, -(\GetScreenHeight() - (gameScreenHeight*scale))*0.5f);
		//SetMouseScale(1/scale, 1/scale);
		//----------------------------------------------------------------------------------

		// Draw
		//----------------------------------------------------------------------------------
		// Draw everything in the render texture, note this will not be rendered on screen, yet
		BeginTextureMode(target);
			// Setup the backbuffer for drawing (clear color and depth buffers)
			ClearBackground(BLACK);

			HandleInput();
			DDANonLinear(rays, player.position, player.rotation);
			if (drawMode == GAME || drawMode == GAME_DEBUG)
			{
				Draw3D(rays, checkerboard64);
			}
			else if (drawMode == MAP || drawMode == MAP_DEBUG)
			{
				Draw2D(rays);
			}
			
			if (drawMode == GAME_DEBUG || drawMode == MAP_DEBUG) { DrawDebug(); }

			//DrawText(TextFormat("Default Mouse: [%i , %i]", (int)mouse.x, (int)mouse.y), 350, 25, 20, GREEN);
			//DrawText(TextFormat("Virtual Mouse: [%i , %i]", (int)virtualMouse.x, (int)virtualMouse.y), 350, 55, 20, YELLOW);
		EndTextureMode();

		BeginDrawing();
			// Clear screen background
			ClearBackground(BLACK);     
			// Draw render texture to screen, properly scaled
			DrawTexturePro(
				target.texture, 
				(Rectangle) { 
					0.0f, 
					0.0f, 
					(float)target.texture.width, 
					(float)-target.texture.height 
				},
				(Rectangle) {
					(GetScreenWidth() - ((float)VIEWPORT_WIDTH * scale)) * 0.5f,
					(GetScreenHeight() - ((float)VIEWPORT_HEIGHT * scale)) * 0.5f,
					(float)VIEWPORT_WIDTH * scale, 
					(float)VIEWPORT_HEIGHT * scale
				}, 
				(Vector2) { 0, 0 },
				0.0f,
				WHITE
			);
		// end the frame and get ready for the next one  (display frame, poll input, etc...)
		EndDrawing();
		//--------------------------------------------------------------------------------------
	}

	// De-Initialization
	//--------------------------------------------------------------------------------------
	// Unload render texture
	UnloadRenderTexture(target);        
	// Unload remaining textures
	UnloadTexture(wabbit);
	UnloadTexture(checkerboard);
	UnloadTexture(checkerboard2);
	UnloadTexture(greyBrick);
	UnloadTexture(redBrick);
	UnloadTexture(metal);
	UnloadTexture(texCoordsDebug);

	// destory the window and cleanup the OpenGL context
	CloseWindow();
	//--------------------------------------------------------------------------------------
	return 0;
}