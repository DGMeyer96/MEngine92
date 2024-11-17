#pragma once

#include "raylib.h"
#include "raymath.h"

#include <stdlib.h> 

#define MAX(a, b) ((a)>(b)? (a) : (b))
#define MIN(a, b) ((a)<(b)? (a) : (b))

enum DrawMode {
	GAME,
	MAP
};

enum Resolution {
	VERY_LOW,
	LOW,
	MEDIUM,
	HIGH,
	ULTRA
};

struct MyRay {
	Vector2 start;
	Vector2 end;
	float distance;
	bool hitX;
	float castAngleRadians;
	float offset;
} typedef MyRay;

struct Player {
	Vector2 position;
	float rotation;
	float moveSpeed;
	float rotateSpeed;
	float colliderRadius;
	Rectangle collisionRect;
} typedef Player;

Vector2 rotateAroundPoint(Vector2 origin, Vector2 point, float angleDegrees)
{
	Vector2 result = Vector2Subtract(point, origin);
	result = Vector2Rotate(result, angleDegrees * DEG2RAD);
	result = Vector2Add(result, origin);

	return result;
}

Vector2 forwardVector(Vector2 point, float angleDegrees)
{
	Vector2 result = (Vector2){
		cosf(angleDegrees * DEG2RAD),
		sinf(angleDegrees * DEG2RAD)
	};

	return result;
}