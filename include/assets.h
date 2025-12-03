#pragma once

#include "raylib.h"
#include "maze.h"
#include <stdbool.h>

// Texture assets
typedef struct {
    Texture2D wallTexture;
    Texture2D floorTexture;
    Texture2D ceilingTexture;
    Music horrorMusic;
    Sound jumpScareSound;
    Sound footstepSound;
    Sound batteryPickupSound;
    Sound bulletSound;
    bool loaded;
} GameAssets;

// Torch structure
typedef struct {
    Vector3 position;      // World position
    Vector3 normal;        // Wall normal (for orientation)
    float flickerTime;     // Time accumulator for flickering
    float baseIntensity;   // Base light intensity
} Torch;

// Particle for flame effect
typedef struct {
    Vector3 position;
    Vector3 velocity;
    float life;
    float maxLife;
    float size;
    Color color;
} Particle;

// Particle system for torch flames
typedef struct {
    Particle* particles;
    int maxParticles;
    int activeParticles;
    Vector3 emitterPos;
    float emitRate;
    float emitAccumulator;
} ParticleSystem;

// Battery pickup structure
typedef struct {
    Vector3 position;      // World position
    bool collected;        // Whether this battery has been collected
    float bobTime;         // Time accumulator for bobbing animation
    float rotation;        // Rotation angle for visual effect
} BatteryPickup;

// Function declarations
GameAssets* Assets_Load(void);
void Assets_Unload(GameAssets* assets);
Texture2D GenerateStoneWallTexture(int width, int height);
Texture2D GenerateWoodFloorTexture(int width, int height);
Texture2D GenerateCeilingTexture(int width, int height);

// Torch functions
int Torches_Generate(const Maze* maze, Torch** outTorches, int maxTorches);
void Torches_Update(Torch* torches, int count, float dt);
void Torches_Render(const Torch* torches, int count);

// Particle system functions
ParticleSystem* ParticleSystem_Create(int maxParticles);
void ParticleSystem_Destroy(ParticleSystem* ps);
void ParticleSystem_Update(ParticleSystem* ps, Vector3 emitterPos, float dt);
void ParticleSystem_Render(const ParticleSystem* ps);

// Lighting functions
void Lighting_UpdateTorchLights(const Torch* torches, int count, float time);

// Battery pickup functions
int Batteries_Generate(const Maze* maze, BatteryPickup** outBatteries, int maxBatteries);
void Batteries_Update(BatteryPickup* batteries, int count, float dt);
void Batteries_Render(const BatteryPickup* batteries, int count, float globalTime);

