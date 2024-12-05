#include "helpful_math.h"

Vector2 Vector2RotateAroundPoint(Vector2 origin, Vector2 point, float angleDegrees)
{
	Vector2 result = Vector2Subtract(point, origin);
	result = Vector2Rotate(result, angleDegrees * DEG2RAD);
	result = Vector2Add(result, origin);

	return result;
}

Vector2 Vector2Forward(float angleDegrees)
{
	return (Vector2) {
		cosf(angleDegrees * DEG2RAD),
			sinf(angleDegrees * DEG2RAD)
	};
}