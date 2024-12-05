#pragma once

#include "raymath.h"

#define MAX(a, b) ((a)>(b)? (a) : (b))
#define MIN(a, b) ((a)<(b)? (a) : (b))

Vector2 Vector2RotateAroundPoint(Vector2 origin, Vector2 point, float angleDegrees);
Vector2 Vector2Forward(float angleDegrees);