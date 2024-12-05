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
#include "raymath.h"
#include "renderer.h"
#include "player.h"

#include "resource_dir.h"			// utility header for SearchAndSetResourceDir
#include <stdio.h>                  // Required for: fopen(), fclose(), fputc(), fwrite(), printf(), fprintf(), funopen()
#include <time.h>                   // Required for: time_t, tm, time(), localtime(), strftime()
#include <math.h>					// Need Math extensions

#define MAP_LENGTH 10

const unsigned int map[MAP_LENGTH][MAP_LENGTH] = {
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

int main ()
{
	SetTraceLogLevel(LOG_ALL);

	CreateRenderer(0, 1, 1280, 960, 90, map);
	CreatePlayer((Vector2) { 1.5, 1.5 }, 0.0, 2.0, 90.0, 0.2, map);
	
	
	// game loop
	while (!WindowShouldClose())		// run the loop untill the user presses ESCAPE or presses the Close button on the window
	{
		PlayerInput();
		RendererInput();

		UpdateRenderCamera(player.position, player.rotation);

		UpdateFrameBuffer();
		UpdateScreen();
	}

	UnloadTextures();

	// destory the window and cleanup the OpenGL context
	CloseWindow();
	//--------------------------------------------------------------------------------------
	return 0;
}