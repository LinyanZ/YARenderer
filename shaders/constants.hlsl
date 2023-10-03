#ifndef __CONSTANTS_HLSL__
#define __CONSTANTS_HLSL__

#define POINT_LIGHT 0
#define SPOT_LIGHT 1
#define DIRECTIONAL_LIGHT 2

static const uint VOXEL_DIMENSION = 256;
static const float VOXEL_GRID_SIZE = 0.1;
static const float3 VOXEL_GRID_WORLD_POS = 0;
static const float VOXEL_COMPRESS_COLOR_RANGE = 10.0;

static const float PI = 3.141592653589793;
static const float TwoPI = 2 * PI;
static const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04;

#endif // __CONSTANTS_HLSL__