#include "raylib.h"
#include "../include/maze.h"
#include "../include/assets.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Game Constants
#define MAZE_WIDTH           8     // Number of cells horizontally
#define MAZE_HEIGHT          8      // Number of cells vertically
#define CELL_SIZE            3.0f    // Size of each cell in world units
#define WALL_THICK           0.2f    // Wall thickness for rendering
#define WALL_HEIGHT          4.0f    // Height of walls

#define PLAYER_RADIUS        0.30f   // Collision radius (XZ)
#define PLAYER_EYE_HEIGHT    1.80f   // Camera height above "feet"
#define GRAVITY             -18.0f
#define JUMP_SPEED           6.5f
#define MOVE_SPEED           5.0f
#define RUN_MULTIPLIER       1.8f
#define MOUSE_SENS           0.0020f // Radians per pixel

#define SCARY_CHAR_COUNT     5       // Number of scary characters
#define SCARY_CHAR_SPEED     2.8f    // Scary character movement speed
#define SCARY_CHAR_RADIUS    0.35f   // Collision radius
#define SCARY_CHAR_HEIGHT    2.2f    // Height of scary character
#define SCARY_CHAR_RANDOMNESS 0.15f  // Randomness factor in movement
#define SCARY_CHAR_HEALTH    1.0f    // Enemy health points (one-hit kill)
#define SCARY_CHAR_MAX_HEALTH 1.0f   // Maximum enemy health

#define WEAPON_ATTACK_RANGE  1.5f    // Melee attack range
#define WEAPON_SHOOT_RANGE   10.0f   // Shoot/range attack range
#define WEAPON_ATTACK_COOLDOWN 0.6f  // Attack cooldown in seconds
#define WEAPON_KNIFE_DAMAGE   1.0f   // Knife damage per hit (one-hit kill)

#define BEST_RECORD_FILE     "best_record.txt"

// Game state
typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_WON,
    GAME_STATE_GAMEOVER
} GameState;

// Scary character structure
typedef struct {
    Vector3 position;
    float speed;
    float radius;
    float height;
    float health;          // Health points (0 = dead)
    float maxHealth;        // Maximum health
    bool isDead;            // Death state
} ScaryCharacter;

// Weapon types
typedef enum {
    WEAPON_NONE,
    WEAPON_GUN
} WeaponType;

// Weapon structure
typedef struct {
    WeaponType type;
    float damage;           // Damage per hit
    float attackRange;      // Attack range
    float attackCooldown;   // Time between attacks
    float durability;      // Weapon durability (0-100)
} Weapon;

// Weapon pickup structure
typedef struct {
    Vector3 position;       // World position
    WeaponType type;        // Weapon type
    bool collected;         // Whether collected
    float bobTime;          // Bobbing animation
    float rotation;         // Rotation animation
} WeaponPickup;

// Dust particle for flashlight beam volumetric effect
typedef struct {
    Vector3 position;
    Vector3 velocity;
    float life;
    float maxLife;
    float size;
    float distance;  // Distance along beam
} DustParticle;

// Dust particle system for flashlight
typedef struct {
    DustParticle* particles;
    int maxParticles;
    int activeParticles;
    float emitAccumulator;  // Accumulator for particle emission
} FlashlightDustSystem;

// Helper function: Clamp float value
static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Get weapon stats based on type
static Weapon GetWeaponStats(WeaponType type) {
    Weapon weapon = {0};
    weapon.type = type;
    
    switch (type) {
        case WEAPON_GUN:
            weapon.damage = 1.0f;  // One-hit kill
            weapon.attackRange = WEAPON_SHOOT_RANGE;  // Long range for gun
            weapon.attackCooldown = WEAPON_ATTACK_COOLDOWN;
            weapon.durability = 100.0f;
            break;
        default:
            weapon.damage = 0.0f;
            weapon.attackRange = 0.0f;
            weapon.attackCooldown = 0.0f;
            weapon.durability = 0.0f;
            break;
    }
    
    return weapon;
}

// Generate weapon pickups in the maze
static int Weapons_Generate(const Maze* maze, WeaponPickup** outWeapons, int maxWeapons) {
    if (!maze || !outWeapons || maxWeapons <= 0) return 0;
    
    *outWeapons = (WeaponPickup*)malloc(maxWeapons * sizeof(WeaponPickup));
    if (!*outWeapons) return 0;
    
    int count = 0;
    const float weaponHeight = 0.6f;
    const float weaponPlacementChance = 0.08f; // 8% chance per cell
    
    for (int y = 0; y < maze->height && count < maxWeapons; y++) {
        for (int x = 0; x < maze->width && count < maxWeapons; x++) {
            bool isStart = (x == (int)maze->startPos.x && y == (int)maze->startPos.y);
            bool isExit = (x == (int)maze->exitPos.x && y == (int)maze->exitPos.y);
            
            if (isStart || isExit) continue;
            
            if ((float)rand() / (float)RAND_MAX < weaponPlacementChance) {
                Vector2 worldPos = Maze_CellToWorld(maze, x, y);
                
                float offsetX = ((float)(rand() % 100) / 100.0f - 0.5f) * maze->cellSize * 0.6f;
                float offsetZ = ((float)(rand() % 100) / 100.0f - 0.5f) * maze->cellSize * 0.6f;
                
                (*outWeapons)[count].position = (Vector3){
                    worldPos.x + offsetX,
                    weaponHeight,
                    worldPos.y + offsetZ
                };
                
                // All weapons are guns
                (*outWeapons)[count].type = WEAPON_GUN;
                
                (*outWeapons)[count].collected = false;
                (*outWeapons)[count].bobTime = (float)(rand() % 1000) / 1000.0f * 6.28f;
                (*outWeapons)[count].rotation = (float)(rand() % 360) * DEG2RAD;
                
                count++;
            }
        }
    }
    
    return count;
}

// Update weapon pickups
static void Weapons_Update(WeaponPickup* weapons, int count, float dt) {
    if (!weapons) return;
    
    for (int i = 0; i < count; i++) {
        if (!weapons[i].collected) {
            weapons[i].bobTime += dt * 2.0f;
            if (weapons[i].bobTime > 6.28f) {
                weapons[i].bobTime -= 6.28f;
            }
            
            weapons[i].rotation += dt * 2.0f;
            if (weapons[i].rotation > 6.28f) {
                weapons[i].rotation -= 6.28f;
            }
        }
    }
}

// Render weapon pickups
static void Weapons_Render(const WeaponPickup* weapons, int count, float globalTime) {
    if (!weapons) return;
    
    for (int i = 0; i < count; i++) {
        if (weapons[i].collected) continue;
        
        Vector3 pos = weapons[i].position;
        float bobOffset = sinf(weapons[i].bobTime) * 0.1f;
        pos.y = weapons[i].position.y + bobOffset;
        
        // Render gun
        Color weaponColor = (Color){60, 60, 70, 255}; // Dark gray/black for gun
        float weaponSize = 0.15f;
        
        if (weapons[i].type == WEAPON_GUN) {
            // Draw gun body (horizontal)
            DrawCube(pos, weaponSize * 1.5f, weaponSize * 0.4f, weaponSize * 0.3f, weaponColor);
            // Draw gun barrel (extending forward)
            Vector3 barrelPos = pos;
            barrelPos.x += weaponSize * 0.6f;
            DrawCube(barrelPos, weaponSize * 0.8f, weaponSize * 0.3f, weaponSize * 0.25f, (Color){40, 40, 50, 255});
            // Draw gun grip
            Vector3 gripPos = pos;
            gripPos.x -= weaponSize * 0.3f;
            gripPos.y -= weaponSize * 0.3f;
            DrawCube(gripPos, weaponSize * 0.4f, weaponSize * 0.6f, weaponSize * 0.3f, (Color){80, 50, 30, 255});
        }
        
        // Glow effect
        float pulse = 0.7f + 0.3f * sinf(globalTime * 2.0f + i);
        Color glowColor = (Color){
            weaponColor.r,
            weaponColor.g,
            weaponColor.b,
            (unsigned char)(80 * pulse)
        };
        DrawSphere(pos, weaponSize * 1.2f * pulse, glowColor);
    }
}

// Create flashlight dust particle system
static FlashlightDustSystem* FlashlightDust_Create(int maxParticles) {
    FlashlightDustSystem* dust = (FlashlightDustSystem*)malloc(sizeof(FlashlightDustSystem));
    if (!dust) return NULL;
    
    dust->particles = (DustParticle*)malloc(maxParticles * sizeof(DustParticle));
    if (!dust->particles) {
        free(dust);
        return NULL;
    }
    
    dust->maxParticles = maxParticles;
    dust->activeParticles = 0;
    dust->emitAccumulator = 0.0f;
    
    return dust;
}

// Destroy flashlight dust system
static void FlashlightDust_Destroy(FlashlightDustSystem* dust) {
    if (!dust) return;
    if (dust->particles) free(dust->particles);
    free(dust);
}

// Update flashlight dust particles
static void FlashlightDust_Update(FlashlightDustSystem* dust, Vector3 flashlightPos, Vector3 flashlightDir, 
                                   bool flashlightOn, float dt) {
    if (!dust) return;
    
    if (!flashlightOn) {
        // Clear particles when flashlight is off
        dust->activeParticles = 0;
        dust->emitAccumulator = 0.0f;
        return;
    }
    
    const float FLASHLIGHT_RANGE = 15.0f;
    const float CONE_ANGLE = 30.0f; // degrees
    const float CONE_RADIUS_MAX = FLASHLIGHT_RANGE * tanf(DEG2RAD * CONE_ANGLE);
    const float EMIT_RATE = 25.0f; // particles per second
    
    // Reset accumulator if flashlight was just turned off
    if (!flashlightOn) {
        dust->emitAccumulator = 0.0f;
        return;
    }
    
    // Emit new particles
    dust->emitAccumulator += EMIT_RATE * dt;
    int toEmit = (int)dust->emitAccumulator;
    dust->emitAccumulator -= (float)toEmit;
    
    for (int i = 0; i < toEmit && dust->activeParticles < dust->maxParticles; i++) {
        DustParticle* p = &dust->particles[dust->activeParticles];
        
        // Spawn particle at random distance along beam (closer to player for better visibility)
        float spawnDistance = ((float)(rand() % 100) / 100.0f) * FLASHLIGHT_RANGE * 0.8f;
        
        // Calculate position along beam with random offset in cone
        Vector3 basePos = (Vector3){
            flashlightPos.x + flashlightDir.x * spawnDistance,
            flashlightPos.y + flashlightDir.y * spawnDistance,
            flashlightPos.z + flashlightDir.z * spawnDistance
        };
        
        // Get perpendicular vectors to flashlight direction using cross product
        Vector3 worldUp = (Vector3){0, 1, 0};
        Vector3 perp1, perp2;
        
        // If flashlight is pointing straight up/down, use different reference
        if (fabsf(flashlightDir.y) > 0.99f) {
            Vector3 worldRight = (Vector3){1, 0, 0};
            // perp1 = worldRight x flashlightDir
            perp1 = (Vector3){
                worldRight.y * flashlightDir.z - worldRight.z * flashlightDir.y,
                worldRight.z * flashlightDir.x - worldRight.x * flashlightDir.z,
                worldRight.x * flashlightDir.y - worldRight.y * flashlightDir.x
            };
        } else {
            // perp1 = worldUp x flashlightDir
            perp1 = (Vector3){
                worldUp.y * flashlightDir.z - worldUp.z * flashlightDir.y,
                worldUp.z * flashlightDir.x - worldUp.x * flashlightDir.z,
                worldUp.x * flashlightDir.y - worldUp.y * flashlightDir.x
            };
        }
        
        // Normalize perp1
        float len1 = sqrtf(perp1.x*perp1.x + perp1.y*perp1.y + perp1.z*perp1.z);
        if (len1 > 0.001f) {
            perp1.x /= len1; perp1.y /= len1; perp1.z /= len1;
        }
        
        // perp2 = flashlightDir x perp1
        perp2 = (Vector3){
            flashlightDir.y * perp1.z - flashlightDir.z * perp1.y,
            flashlightDir.z * perp1.x - flashlightDir.x * perp1.z,
            flashlightDir.x * perp1.y - flashlightDir.y * perp1.x
        };
        
        // Normalize perp2
        float len2 = sqrtf(perp2.x*perp2.x + perp2.y*perp2.y + perp2.z*perp2.z);
        if (len2 > 0.001f) {
            perp2.x /= len2; perp2.y /= len2; perp2.z /= len2;
        }
        
        // Random angle and radius in cone
        float angle = ((float)(rand() % 1000) / 1000.0f) * 2.0f * PI;
        float radius = ((float)(rand() % 100) / 100.0f) * CONE_RADIUS_MAX * (spawnDistance / FLASHLIGHT_RANGE);
        
        p->position = (Vector3){
            basePos.x + (perp1.x * cosf(angle) + perp2.x * sinf(angle)) * radius,
            basePos.y + (perp1.y * cosf(angle) + perp2.y * sinf(angle)) * radius,
            basePos.z + (perp1.z * cosf(angle) + perp2.z * sinf(angle)) * radius
        };
        
        // Slow drift velocity (random, gentle movement)
        p->velocity = (Vector3){
            ((float)(rand() % 100) - 50.0f) / 500.0f,
            ((float)(rand() % 100) - 50.0f) / 500.0f,
            ((float)(rand() % 100) - 50.0f) / 500.0f
        };
        
        p->distance = spawnDistance;
        p->life = 1.0f;
        p->maxLife = 3.0f + ((float)(rand() % 100) / 100.0f) * 2.0f; // 3-5 seconds
        p->size = 0.005f + ((float)(rand() % 10) / 1000.0f); // 0.005-0.015 (much smaller, more realistic)
        
        dust->activeParticles++;
    }
    
    // Update existing particles
    for (int i = 0; i < dust->activeParticles; i++) {
        DustParticle* p = &dust->particles[i];
        
        // Update position with drift
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->position.z += p->velocity.z * dt;
        
        // Update life
        p->life -= dt;
        
        // Remove dead particles
        if (p->life <= 0.0f) {
            dust->particles[i] = dust->particles[dust->activeParticles - 1];
            dust->activeParticles--;
            i--;
        }
    }
}

// Render flashlight dust particles with volumetric effect
static void FlashlightDust_Render(const FlashlightDustSystem* dust, Vector3 flashlightPos, Vector3 flashlightDir) {
    if (!dust || dust->activeParticles == 0) return;
    
    const float FLASHLIGHT_RANGE = 15.0f;
    
    for (int i = 0; i < dust->activeParticles; i++) {
        const DustParticle* p = &dust->particles[i];
        
        // Calculate distance from flashlight
        float dx = p->position.x - flashlightPos.x;
        float dy = p->position.y - flashlightPos.y;
        float dz = p->position.z - flashlightPos.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        // Check if particle is within flashlight cone
        if (dist > 0.001f) {
            Vector3 toParticle = (Vector3){dx/dist, dy/dist, dz/dist};
            float dot = flashlightDir.x * toParticle.x + flashlightDir.y * toParticle.y + flashlightDir.z * toParticle.z;
            float coneAngle = cosf(DEG2RAD * 30.0f);
            
            if (dot >= coneAngle && dist <= FLASHLIGHT_RANGE) {
                // Calculate alpha based on life and distance
                float lifeAlpha = p->life / p->maxLife;
                float distanceAlpha = 1.0f - clampf(dist / FLASHLIGHT_RANGE, 0.0f, 1.0f);
                float angleFactor = (dot - coneAngle) / (1.0f - coneAngle); // Brighter in center
                
                // More solid/visible alpha while keeping small size
                float alpha = lifeAlpha * distanceAlpha * angleFactor * 0.85f; // Increased for visibility
                
                // Bright white-gray color for better visibility
                Color dustColor = (Color){
                    240 + (rand() % 15),  // Bright white-gray
                    240 + (rand() % 15),
                    235 + (rand() % 20),
                    (unsigned char)(alpha * 255)
                };
                
                // Draw dust particle as small but solid sphere
                DrawSphere(p->position, p->size, dustColor);
            }
        }
    }
}

// Circle (player) vs axis-aligned rectangle (wall) collision in XZ plane
static bool CircleRectIntersect(Vector2 c, float r, Rectangle rect) {
    float nx = clampf(c.x, rect.x, rect.x + rect.width);
    float nz = clampf(c.y, rect.y, rect.y + rect.height);
    float dx = c.x - nx;
    float dz = c.y - nz;
    return (dx*dx + dz*dz) <= r*r;
}

// Circle vs circle collision in XZ plane
static bool CircleCircleIntersect(Vector2 c1, float r1, Vector2 c2, float r2) {
    float dx = c1.x - c2.x;
    float dz = c1.y - c2.y;
    float distSq = dx*dx + dz*dz;
    float radiusSum = r1 + r2;
    return distSq <= radiusSum * radiusSum;
}

// Check collision with any wall rectangle
static bool CollidesAny(Vector2 c, float r, const WallRect* walls, int count) {
    for (int i = 0; i < count; ++i) {
        if (CircleRectIntersect(c, r, walls[i].rect)) return true;
    }
    return false;
}

// Load best record from file
static float LoadBestRecord(void) {
    FILE* file = fopen(BEST_RECORD_FILE, "r");
    if (file) {
        float bestTime = 0.0f;
        if (fscanf(file, "%f", &bestTime) == 1) {
            fclose(file);
            return bestTime;
        }
        fclose(file);
    }
    return -1.0f;
}

// Save best record to file
static void SaveBestRecord(float time) {
    FILE* file = fopen(BEST_RECORD_FILE, "w");
    if (file) {
        fprintf(file, "%.2f", time);
        fclose(file);
    }
}

// Initialize game
static void InitGame(Maze** maze, WallRect** walls, int* wallCount, Vector3* playerPos, 
                     float* yaw, float* pitch, GameState* gameState,
                     Torch** torches, int* torchCount, ParticleSystem*** particleSystems,
                     ScaryCharacter* scaryChars, int scaryCharCount, float* gameTimer,
                     BatteryPickup** batteries, int* batteryCount,
                     WeaponPickup** weapons, int* weaponCount) {
    // Free old maze if it exists
    if (*maze) {
        Maze_Destroy(*maze);
        *maze = NULL;
    }
    if (*walls) {
        free(*walls);
        *walls = NULL;
    }
    if (*torches) {
        free(*torches);
        *torches = NULL;
    }
    if (*particleSystems) {
        for (int i = 0; i < *torchCount; i++) {
            if ((*particleSystems)[i]) {
                ParticleSystem_Destroy((*particleSystems)[i]);
            }
        }
        free(*particleSystems);
        *particleSystems = NULL;
    }
    if (*batteries) {
        free(*batteries);
        *batteries = NULL;
    }
    if (*weapons) {
        free(*weapons);
        *weapons = NULL;
    }
    
    // Create and generate a new maze
    *maze = Maze_Create(MAZE_WIDTH, MAZE_HEIGHT, CELL_SIZE);
    if (!*maze) {
        TraceLog(LOG_ERROR, "Failed to create maze!");
        return;
    }
    
    Maze_Generate(*maze);
    
    // Allocate wall rectangles
    int maxWalls = MAZE_WIDTH * MAZE_HEIGHT * 4;
    *walls = (WallRect*)malloc(maxWalls * sizeof(WallRect));
    if (!*walls) {
        TraceLog(LOG_ERROR, "Failed to allocate wall rectangles!");
        return;
    }
    
    *wallCount = Maze_GetWallRects(*maze, *walls, maxWalls);
    
    // Generate torches (sparse random placement for scary atmosphere)
    int maxTorches = 25;
    *torchCount = Torches_Generate(*maze, torches, maxTorches);
    
    // Create particle systems for each torch
    if (*torchCount > 0) {
        *particleSystems = (ParticleSystem**)malloc(*torchCount * sizeof(ParticleSystem*));
        if (*particleSystems) {
            for (int i = 0; i < *torchCount; i++) {
                (*particleSystems)[i] = ParticleSystem_Create(20);
            }
        }
    }
    
    // Generate battery pickups
    int maxBatteries = 15;
    *batteryCount = Batteries_Generate(*maze, batteries, maxBatteries);
    
    // Generate weapon pickups
    int maxWeapons = 8;
    *weaponCount = Weapons_Generate(*maze, weapons, maxWeapons);
    
    // Reset player to the start position
    Vector2 startWorld = Maze_CellToWorld(*maze, (int)(*maze)->startPos.x, (int)(*maze)->startPos.y);
    playerPos->x = startWorld.x;
    playerPos->y = 0.0f;
    playerPos->z = startWorld.y;
    
    // Initialize scary characters at random positions
    if (scaryChars && scaryCharCount > 0) {
        Vector2 playerStartWorld = Maze_CellToWorld(*maze, (int)(*maze)->startPos.x, (int)(*maze)->startPos.y);
        
        const float MIN_DISTANCE_FROM_PLAYER = 30.0f;
        
        for (int i = 0; i < scaryCharCount; i++) {
            int attempts = 0;
            int cellX, cellY;
            bool validPos = false;
            
            while (!validPos && attempts < 200) {
                cellX = rand() % (*maze)->width;
                cellY = rand() % (*maze)->height;
                
                bool isStart = (cellX == (int)(*maze)->startPos.x && cellY == (int)(*maze)->startPos.y);
                bool isExit = (cellX == (int)(*maze)->exitPos.x && cellY == (int)(*maze)->exitPos.y);
                
                bool isDuplicate = false;
                for (int j = 0; j < i; j++) {
                    int existingCellX, existingCellY;
                    Maze_WorldToCell(*maze, scaryChars[j].position.x, scaryChars[j].position.z, &existingCellX, &existingCellY);
                    if (cellX == existingCellX && cellY == existingCellY) {
                        isDuplicate = true;
                        break;
                    }
                }
                
                Vector2 charWorldPos = Maze_CellToWorld(*maze, cellX, cellY);
                float dx = charWorldPos.x - playerStartWorld.x;
                float dz = charWorldPos.y - playerStartWorld.y;
                float distFromPlayer = sqrtf(dx * dx + dz * dz);
                bool isFarEnough = (distFromPlayer >= MIN_DISTANCE_FROM_PLAYER);
                
                if (!isStart && !isExit && !isDuplicate && isFarEnough) {
                    validPos = true;
                }
                attempts++;
            }
            
            if (!validPos) {
                const float RELAXED_DISTANCE = 25.0f;
                for (int retry = 0; retry < 50; retry++) {
                    cellX = rand() % (*maze)->width;
                    cellY = rand() % (*maze)->height;
                    Vector2 charWorldPos = Maze_CellToWorld(*maze, cellX, cellY);
                    float dx = charWorldPos.x - playerStartWorld.x;
                    float dz = charWorldPos.y - playerStartWorld.y;
                    float distFromPlayer = sqrtf(dx * dx + dz * dz);
                    if (distFromPlayer >= RELAXED_DISTANCE) {
                        validPos = true;
                        break;
                    }
                }
                if (!validPos) {
                    cellX = rand() % (*maze)->width;
                    cellY = rand() % (*maze)->height;
                }
            }
            
            Vector2 worldPos = Maze_CellToWorld(*maze, cellX, cellY);
            scaryChars[i].position = (Vector3){worldPos.x, 0.0f, worldPos.y};
            scaryChars[i].speed = SCARY_CHAR_SPEED;
            scaryChars[i].radius = SCARY_CHAR_RADIUS;
            scaryChars[i].height = SCARY_CHAR_HEIGHT;
            scaryChars[i].health = SCARY_CHAR_HEALTH;
            scaryChars[i].maxHealth = SCARY_CHAR_MAX_HEALTH;
            scaryChars[i].isDead = false;
        }
    }
    
    *yaw = 0.0f;
    *pitch = 0.0f;
    *gameState = GAME_STATE_PLAYING;
    *gameTimer = 0.0f;
}

// Static cube model
static Model s_cubeModel = {0};
static bool s_cubeModelCreated = false;

static Model* GetCubeModel(void) {
    if (!s_cubeModelCreated) {
        Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        s_cubeModel = LoadModelFromMesh(cubeMesh);
        s_cubeModelCreated = true;
    }
    return &s_cubeModel;
}

static void CleanupCubeModel(void) {
    if (s_cubeModelCreated && s_cubeModel.meshCount > 0) {
        UnloadModel(s_cubeModel);
        s_cubeModel.meshCount = 0;
        s_cubeModelCreated = false;
    }
}

static Texture2D s_currentCubeTexture = {0};
static void DrawTexturedCube(Vector3 position, Vector3 size, Texture2D texture) {
    Model* cubeModel = GetCubeModel();
    
    if (s_currentCubeTexture.id != texture.id) {
        cubeModel->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        s_currentCubeTexture = texture;
    }
    
    DrawModelEx(*cubeModel, position, (Vector3){0, 1, 0}, 0.0f, size, WHITE);
}

// Static plane model
static Model s_planeModel = {0};
static bool s_planeModelCreated = false;

static Model* GetPlaneModel(void) {
    if (!s_planeModelCreated) {
        Mesh planeMesh = GenMeshPlane(1.0f, 1.0f, 1, 1);
        s_planeModel = LoadModelFromMesh(planeMesh);
        s_planeModelCreated = true;
    }
    return &s_planeModel;
}

static Texture2D s_currentPlaneTexture = {0};
static void DrawTexturedPlane(Vector3 position, Vector2 size, Texture2D texture) {
    Model* planeModel = GetPlaneModel();
    
    if (s_currentPlaneTexture.id != texture.id) {
        planeModel->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        s_currentPlaneTexture = texture;
    }
    
    DrawModelEx(*planeModel, position, (Vector3){0, 1, 0}, 0.0f, (Vector3){size.x, 1.0f, size.y}, WHITE);
}

static void CleanupPlaneModel(void) {
    if (s_planeModelCreated && s_planeModel.meshCount > 0) {
        UnloadModel(s_planeModel);
        s_planeModel.meshCount = 0;
        s_planeModelCreated = false;
    }
}

// Calculate light intensity at a given position from all torches and flashlight
static float CalculateLightIntensity(Vector3 pos, const Torch* torches, int torchCount, 
                                     Vector3 flashlightPos, Vector3 flashlightDir, bool flashlightOn, float flashlightBattery) {
    float maxIntensity = 0.0f;
    
    // Calculate torch lighting
    if (torches && torchCount > 0) {
        for (int i = 0; i < torchCount; i++) {
            // Calculate distance to torch
            float dx = pos.x - torches[i].position.x;
            float dy = pos.y - torches[i].position.y;
            float dz = pos.z - torches[i].position.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            
            // Calculate flicker intensity (subtle, minimal)
            float baseFlicker = 0.92f + 0.08f * sinf(torches[i].flickerTime);
            
            float intensity = torches[i].baseIntensity * baseFlicker;
            
            // Distance falloff (inverse square with max range)
            float lightRange = 3.0f;
            float falloff = 1.0f - clampf(dist / lightRange, 0.0f, 1.0f);
            falloff = falloff * falloff; // Quadratic falloff
            
            float lightIntensity = intensity * falloff;
            if (lightIntensity > maxIntensity) {
                maxIntensity = lightIntensity;
            }
        }
    }
    
    // Calculate flashlight lighting (real illumination)
    if (flashlightOn && flashlightBattery > 0.0f) {
        Vector3 toPos = (Vector3){pos.x - flashlightPos.x, pos.y - flashlightPos.y, pos.z - flashlightPos.z};
        float dist = sqrtf(toPos.x*toPos.x + toPos.y*toPos.y + toPos.z*toPos.z);
        
        if (dist > 0.001f) {
            // Normalize direction to position
            toPos.x /= dist;
            toPos.y /= dist;
            toPos.z /= dist;
            
            // Calculate dot product (angle between flashlight direction and position)
            float dot = flashlightDir.x * toPos.x + flashlightDir.y * toPos.y + flashlightDir.z * toPos.z;
            
            // Flashlight cone (30 degree angle for focused beam)
            float coneAngle = cosf(DEG2RAD * 30.0f);
            if (dot >= coneAngle) {
                // Within cone
                float batteryFactor = flashlightBattery / 100.0f;
                float flashlightRange = 15.0f;  // Long range
                float flashlightIntensity = 5.0f * batteryFactor;  // Much brighter
                
                // Distance falloff
                float falloff = 1.0f - clampf(dist / flashlightRange, 0.0f, 1.0f);
                falloff = falloff * falloff; // Quadratic falloff
                
                // Angle falloff (sharper edges)
                float angleFactor = (dot - coneAngle) / (1.0f - coneAngle);
                angleFactor = angleFactor * angleFactor; // Sharper falloff at edges
                
                float flashlightLight = flashlightIntensity * falloff * angleFactor;
                if (flashlightLight > maxIntensity) {
                    maxIntensity = flashlightLight;
                }
            }
        }
    }
    
    return maxIntensity;
}

// Render the maze in 3D
static void RenderMaze(const Maze* maze, const GameAssets* assets, const Torch* torches, int torchCount, 
                       Vector3 flashlightPos, Vector3 flashlightDir, bool flashlightOn, float flashlightBattery) {
    if (!maze || !assets || !assets->loaded) return;
    
    const float halfCell = maze->cellSize * 0.5f;
    const float wallHalfHeight = WALL_HEIGHT * 0.5f;
    
    float mazeWidth = maze->width * maze->cellSize;
    float mazeHeight = maze->height * maze->cellSize;
    
    // Draw the floor
    DrawTexturedPlane((Vector3){0, 0, 0}, (Vector2){mazeWidth, mazeHeight}, assets->floorTexture);
    
    // Draw the ceiling
    DrawTexturedPlane((Vector3){0, WALL_HEIGHT, 0}, (Vector2){mazeWidth, mazeHeight}, assets->ceilingTexture);
    
    // Add darkening overlays to floor and ceiling in unlit areas
    // Sample points to create gradient darkening
    int floorSamplesX = 15;
    int floorSamplesZ = 15;
    float floorSampleSpacingX = mazeWidth / (float)floorSamplesX;
    float floorSampleSpacingZ = mazeHeight / (float)floorSamplesZ;
    
    for (int sx = 0; sx < floorSamplesX; sx++) {
        for (int sz = 0; sz < floorSamplesZ; sz++) {
            float sampleX = -mazeWidth * 0.5f + sx * floorSampleSpacingX + floorSampleSpacingX * 0.5f;
            float sampleZ = -mazeHeight * 0.5f + sz * floorSampleSpacingZ + floorSampleSpacingZ * 0.5f;
            
            // Check floor lighting
            Vector3 floorPos = (Vector3){sampleX, 0.0f, sampleZ};
            float floorLight = CalculateLightIntensity(floorPos, torches, torchCount, flashlightPos, flashlightDir, flashlightOn, flashlightBattery);
            float floorDarkness = 1.0f - clampf(floorLight * 2.5f, 0.0f, 1.0f);
            
            if (floorDarkness > 0.15f) {
                Color floorDark = (Color){0, 0, 0, (unsigned char)(floorDarkness * 200)};
                DrawPlane((Vector3){sampleX, 0.01f, sampleZ}, 
                         (Vector2){floorSampleSpacingX * 1.1f, floorSampleSpacingZ * 1.1f}, 
                         floorDark);
            }
            
            // Check ceiling lighting
            Vector3 ceilingPos = (Vector3){sampleX, WALL_HEIGHT, sampleZ};
            float ceilingLight = CalculateLightIntensity(ceilingPos, torches, torchCount, flashlightPos, flashlightDir, flashlightOn, flashlightBattery);
            float ceilingDarkness = 1.0f - clampf(ceilingLight * 2.0f, 0.0f, 1.0f);
            
            if (ceilingDarkness > 0.2f) {
                Color ceilingDark = (Color){0, 0, 0, (unsigned char)(ceilingDarkness * 220)};
                DrawPlane((Vector3){sampleX, WALL_HEIGHT - 0.01f, sampleZ}, 
                         (Vector2){floorSampleSpacingX * 1.1f, floorSampleSpacingZ * 1.1f}, 
                         ceilingDark);
            }
        }
    }
    
    // Draw the walls for each cell
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            
            // Calculate light intensity at wall center for darkening effect
            Vector3 wallCenter = (Vector3){worldX, wallHalfHeight, worldZ};
            float lightIntensity = CalculateLightIntensity(wallCenter, torches, torchCount, flashlightPos, flashlightDir, flashlightOn, flashlightBattery);
            float darkness = 1.0f - clampf(lightIntensity * 2.0f, 0.0f, 1.0f);
            
            // Draw the north wall
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                Vector3 wallPos = (Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ - halfCell
                };
                DrawTexturedCube(wallPos, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
                
                // Add darkening overlay if far from light
                if (darkness > 0.2f) {
                    Color darkOverlay = (Color){0, 0, 0, (unsigned char)(darkness * 180)};
                    DrawCube(wallPos, maze->cellSize, WALL_HEIGHT, WALL_THICK + 0.01f, darkOverlay);
                }
            }
            
            // Draw the south wall
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                Vector3 wallPos = (Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ + halfCell
                };
                DrawTexturedCube(wallPos, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
                
                if (darkness > 0.2f) {
                    Color darkOverlay = (Color){0, 0, 0, (unsigned char)(darkness * 180)};
                    DrawCube(wallPos, maze->cellSize, WALL_HEIGHT, WALL_THICK + 0.01f, darkOverlay);
                }
            }
            
            // Draw the west wall
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                Vector3 wallPos = (Vector3){
                    worldX - halfCell,
                    wallHalfHeight,
                    worldZ
                };
                DrawTexturedCube(wallPos, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
                
                if (darkness > 0.2f) {
                    Color darkOverlay = (Color){0, 0, 0, (unsigned char)(darkness * 180)};
                    DrawCube(wallPos, WALL_THICK + 0.01f, WALL_HEIGHT, maze->cellSize, darkOverlay);
                }
            }
            
            // Draw the east wall
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                Vector3 wallPos = (Vector3){
                    worldX + halfCell,
                    wallHalfHeight,
                    worldZ
                };
                DrawTexturedCube(wallPos, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
                
                if (darkness > 0.2f) {
                    Color darkOverlay = (Color){0, 0, 0, (unsigned char)(darkness * 180)};
                    DrawCube(wallPos, WALL_THICK + 0.01f, WALL_HEIGHT, maze->cellSize, darkOverlay);
                }
            }
        }
    }
    
    // Reset the texture tracking for the next frame
    s_currentCubeTexture.id = 0;
    s_currentPlaneTexture.id = 0;
    
    // Highlight the exit cell (green floor)
    Vector2 exitWorld = Maze_CellToWorld(maze, (int)maze->exitPos.x, (int)maze->exitPos.y);
    DrawPlane((Vector3){exitWorld.x, 0.01f, exitWorld.y}, 
              (Vector2){maze->cellSize * 0.8f, maze->cellSize * 0.8f}, 
              (Color){0, 200, 0, 255});
}

int main(void) {
    srand((unsigned int)time(NULL));
    
    // Initialize audio device
    InitAudioDevice();
    
    // Set up the window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "3D Maze Horror");
    SetTargetFPS(120);
    
    bool mouseCaptured = true;
    DisableCursor();
    
    Maze* maze = NULL;
    WallRect* walls = NULL;
    int wallCount = 0;
    GameState gameState = GAME_STATE_MENU;
    
    // Set up the player
    Vector3 playerPos = {0.0f, 0.0f, 0.0f};
    float playerVelY = 0.0f;
    bool onGround = true;
    
    float yaw = 0.0f;
    float pitch = 0.0f;
    const float PITCH_LIMIT = DEG2RAD * 89.0f;
    
    // Set up the camera
    Camera3D cam = {0};
    cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
    cam.target = (Vector3){0, 0, 1};
    cam.up = (Vector3){0, 1, 0};
    cam.fovy = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    
    // Load the assets
    GameAssets* assets = Assets_Load();
    if (!assets) {
        TraceLog(LOG_ERROR, "Failed to load assets!");
        CloseWindow();
        return 1;
    }
    
    // Set up the torches and particle systems
    Torch* torches = NULL;
    int torchCount = 0;
    ParticleSystem** particleSystems = NULL;
    
    // Set up battery pickups
    BatteryPickup* batteries = NULL;
    int batteryCount = 0;
    
    // Set up weapon pickups
    WeaponPickup* weapons = NULL;
    int weaponCount = 0;
    
    // Player weapon state
    Weapon playerWeapon = {0};
    playerWeapon.type = WEAPON_NONE;
    float attackCooldownTimer = 0.0f;
    float attackAnimationTime = 0.0f;
    bool isAttacking = false;
    int bulletCount = 0;  // Number of bullets available
    
    // Set up the scary characters
    ScaryCharacter scaryChars[SCARY_CHAR_COUNT] = {0};
    
    // Set up the timer and best record
    float gameTimer = 0.0f;
    float bestRecord = LoadBestRecord();
    
    // Menu state variables
    bool gameInitialized = false;
    
    // Flashlight system
    bool flashlightOn = false;
    float flashlightBattery = 100.0f;  // Battery level (0-100)
    const float FLASHLIGHT_MAX_BATTERY = 100.0f;
    const float FLASHLIGHT_DRAIN_RATE = 5.0f;  // Battery drain per second when on (20 seconds total)
    const float BATTERY_RECHARGE_AMOUNT = 30.0f;   // How much each battery pickup recharges
    
    // Footstep tracking
    float footstepTimer = 0.0f;
    Vector2 lastPlayerPos2D = {0.0f, 0.0f};
    const float FOOTSTEP_INTERVAL_WALK = 0.5f;  // Time between footsteps when walking
    const float FOOTSTEP_INTERVAL_RUN = 0.3f;   // Time between footsteps when running
    
    // Flashlight dust particle system
    FlashlightDustSystem* flashlightDust = FlashlightDust_Create(150); // Max 150 dust particles
    
        // Start the main game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Update music stream (must be called every frame if music is playing)
        if (assets && assets->horrorMusic.frameCount > 0) {
            UpdateMusicStream(assets->horrorMusic);
        }
        
        // Handle menu state
        if (gameState == GAME_STATE_MENU) {
            EnableCursor();
            if (assets && assets->horrorMusic.frameCount > 0 && IsMusicStreamPlaying(assets->horrorMusic)) {
                StopMusicStream(assets->horrorMusic);
            }
            
            // Start game on Enter or Space
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                // Initialize the game
                InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
                         &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT, &gameTimer,
                         &batteries, &batteryCount, &weapons, &weaponCount);
                
                // Reset weapon state
                playerWeapon.type = WEAPON_NONE;
                attackCooldownTimer = 0.0f;
                attackAnimationTime = 0.0f;
                isAttacking = false;
                bulletCount = 0;
                gameState = GAME_STATE_PLAYING;
                gameInitialized = true;
                mouseCaptured = true;
                DisableCursor();
                
                // Reset footstep tracking
                footstepTimer = 0.0f;
                lastPlayerPos2D = (Vector2){playerPos.x, playerPos.z};
                
                // Reset flashlight
                flashlightOn = false;
                flashlightBattery = FLASHLIGHT_MAX_BATTERY;
                
                // Start playing horror music when game starts
                if (assets && assets->horrorMusic.frameCount > 0) {
                    PlayMusicStream(assets->horrorMusic);
                }
            }
        }
        
        // Toggle the mouse capture (only in playing state)
        if (gameState == GAME_STATE_PLAYING && IsKeyPressed(KEY_F)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) DisableCursor();
            else EnableCursor();
        }
        
        // Toggle flashlight (only in playing state)
        if (gameInitialized && gameState == GAME_STATE_PLAYING && IsKeyPressed(KEY_T)) {
            if (flashlightBattery > 0.0f) {
                flashlightOn = !flashlightOn;
                // Turn off automatically if battery is depleted
                if (flashlightBattery <= 0.0f) {
                    flashlightOn = false;
                }
            } else {
                flashlightOn = false;
            }
        }
        
        // Restart the game (only when game is initialized)
        if (gameInitialized && IsKeyPressed(KEY_R) && 
            (gameState == GAME_STATE_WON || gameState == GAME_STATE_GAMEOVER)) {
            // Stop music before restarting
            if (assets && assets->horrorMusic.frameCount > 0) {
                StopMusicStream(assets->horrorMusic);
            }
            
            // Stop jumpscare sound if playing
            if (assets && assets->jumpScareSound.frameCount > 0 && IsSoundPlaying(assets->jumpScareSound)) {
                StopSound(assets->jumpScareSound);
            }
            
            // Stop victory sound if playing
            if (assets && assets->victorySound.frameCount > 0 && IsSoundPlaying(assets->victorySound)) {
                StopSound(assets->victorySound);
            }
            
            InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
                     &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT, &gameTimer,
                     &batteries, &batteryCount, &weapons, &weaponCount);
            
            // Reset weapon state
            playerWeapon.type = WEAPON_NONE;
            attackCooldownTimer = 0.0f;
            attackAnimationTime = 0.0f;
            isAttacking = false;
            gameState = GAME_STATE_PLAYING;
            mouseCaptured = true;
            DisableCursor();
            
            // Reset footstep tracking
            footstepTimer = 0.0f;
            lastPlayerPos2D = (Vector2){playerPos.x, playerPos.z};
            
            // Reset flashlight
            flashlightOn = false;
            flashlightBattery = FLASHLIGHT_MAX_BATTERY;
            
            // Restart music
            if (assets && assets->horrorMusic.frameCount > 0) {
                PlayMusicStream(assets->horrorMusic);
            }
        }
        // timer update (only when game is initialized)
        if (gameInitialized && gameState == GAME_STATE_PLAYING) {
            gameTimer += dt;
            
            // Update flashlight battery drain
            if (flashlightOn && flashlightBattery > 0.0f) {
                flashlightBattery -= FLASHLIGHT_DRAIN_RATE * dt;
                if (flashlightBattery <= 0.0f) {
                    flashlightBattery = 0.0f;
                    flashlightOn = false;  // Turn off automatically when battery is depleted
                }
            }
        }

        if (gameInitialized && torches && torchCount > 0) {
            Torches_Update(torches, torchCount, dt);
            
            // Update particle systems
            if (particleSystems) {
                for (int i = 0; i < torchCount; i++) {
                    if (particleSystems[i]) {
                        Vector3 flamePos = torches[i].position;
                        flamePos.y += 0.25f; // Offset above torch
                        ParticleSystem_Update(particleSystems[i], flamePos, dt);
                    }
                }
            }
        }
        
        // Update battery pickups
        if (gameInitialized && batteries && batteryCount > 0) {
            Batteries_Update(batteries, batteryCount, dt);
        }
        
        // Update weapon pickups
        if (gameInitialized && weapons && weaponCount > 0) {
            Weapons_Update(weapons, weaponCount, dt);
        }
        
        // Update attack cooldown
        if (gameInitialized && gameState == GAME_STATE_PLAYING) {
            if (attackCooldownTimer > 0.0f) {
                attackCooldownTimer -= dt;
            }
            if (attackAnimationTime > 0.0f) {
                attackAnimationTime -= dt;
                if (attackAnimationTime <= 0.0f) {
                    isAttacking = false;
                }
            }
        }
        
        // mouse look (only when game is initialized and playing)
        if (gameInitialized && mouseCaptured && gameState == GAME_STATE_PLAYING) {
            Vector2 md = GetMouseDelta();
            yaw -= md.x * MOUSE_SENS;
            pitch -= md.y * MOUSE_SENS;
            if (pitch > PITCH_LIMIT) pitch = PITCH_LIMIT;
            if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
        }
        
        // calculate the forward and right vectors
        Vector3 forward = (Vector3){
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
        Vector3 right = (Vector3){cosf(yaw), 0.0f, -sinf(yaw)};
        
        // Update flashlight dust particles (after forward vector is calculated)
        if (gameInitialized && flashlightDust && gameState == GAME_STATE_PLAYING) {
            Vector3 flashlightPos = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
            Vector3 flashlightDir = forward;
            FlashlightDust_Update(flashlightDust, flashlightPos, flashlightDir, flashlightOn, dt);
        }
        
        // player movement input (only when game is initialized)
        if (gameInitialized && gameState == GAME_STATE_PLAYING) {
            float speed = MOVE_SPEED * (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_MULTIPLIER : 1.0f);
            Vector2 wish = (Vector2){0.0f, 0.0f};
            
            if (IsKeyDown(KEY_W)) {
                wish.x += forward.x;
                wish.y += forward.z;
            }
            if (IsKeyDown(KEY_S)) {
                wish.x -= forward.x;
                wish.y -= forward.z;
            }
            if (IsKeyDown(KEY_D)) {
                wish.x -= right.x;
                wish.y -= right.z;
            }
            if (IsKeyDown(KEY_A)) {
                wish.x += right.x;
                wish.y += right.z;
            }
            
            float len = sqrtf(wish.x * wish.x + wish.y * wish.y);
            if (len > 0.0001f) {
                wish.x /= len;
                wish.y /= len;
            }

            Vector2 pXZ = (Vector2){playerPos.x, playerPos.z};
            Vector2 step = (Vector2){wish.x * speed * dt, wish.y * speed * dt};
            
            Vector2 testX = (Vector2){pXZ.x + step.x, pXZ.y};
            if (!CollidesAny(testX, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.x = testX.x;
            }

            Vector2 testZ = (Vector2){pXZ.x, pXZ.y + step.y};
            if (!CollidesAny(testZ, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.y = testZ.y;
            }
            
            playerPos.x = pXZ.x;
            playerPos.z = pXZ.y;

            onGround = (playerPos.y <= 0.0001f);
            
            // Handle footstep sounds - only when WASD keys are pressed
            bool isPressingWASD = (IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D));
            bool isMoving = (len > 0.0001f);
            
            if (isPressingWASD && isMoving && onGround && assets && assets->footstepSound.frameCount > 0) {
                // Determine footstep interval based on speed (walking vs running)
                float footstepInterval = (speed > MOVE_SPEED * 1.5f) ? FOOTSTEP_INTERVAL_RUN : FOOTSTEP_INTERVAL_WALK;
                
                // Update footstep timer
                footstepTimer += dt;
                
                // Play footstep sound at intervals
                if (footstepTimer >= footstepInterval) {
                    PlaySound(assets->footstepSound);
                    footstepTimer = 0.0f;
                }
                
                // Update last position
                lastPlayerPos2D = pXZ;
            } else {
                // Stop footstep sound if currently playing when player stops or releases keys
                if (assets && assets->footstepSound.frameCount > 0 && IsSoundPlaying(assets->footstepSound)) {
                    StopSound(assets->footstepSound);
                }
                
                // Reset timer when not moving or keys released
                footstepTimer = 0.0f;
                lastPlayerPos2D = pXZ;
            }
            
            if (onGround) {
                playerPos.y = 0.0f;
                playerVelY = 0.0f;
                if (IsKeyPressed(KEY_SPACE)) {
                    playerVelY = JUMP_SPEED;
                    onGround = false;
                }
            } else {
                playerVelY += GRAVITY * dt;
            }
            playerPos.y += playerVelY * dt;
            
            float maxFeetY = WALL_HEIGHT - PLAYER_EYE_HEIGHT;
            if (playerPos.y > maxFeetY) {
                playerPos.y = maxFeetY;
                if (playerVelY > 0) playerVelY = 0;
            }
            
            // Handle shoot input (left or right mouse button - gun)
            if (playerWeapon.type != WEAPON_NONE && bulletCount > 0 && 
                (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && 
                !isAttacking) {
                isAttacking = true;
                attackAnimationTime = 0.3f; // Attack animation duration
                bulletCount--; // Use one bullet
                
                // Play bullet sound
                if (assets && assets->bulletSound.frameCount > 0) {
                    PlaySound(assets->bulletSound);
                }
                
                // Check for enemy hits (shoot range)
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                
                for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                    if (scaryChars[i].isDead) continue;
                    
                    Vector2 enemyPos2D = (Vector2){scaryChars[i].position.x, scaryChars[i].position.z};
                    float dist = sqrtf((playerPos2D.x - enemyPos2D.x) * (playerPos2D.x - enemyPos2D.x) + 
                                      (playerPos2D.y - enemyPos2D.y) * (playerPos2D.y - enemyPos2D.y));
                    
                    // Check if enemy is in shoot range
                    if (dist <= playerWeapon.attackRange) {
                        // One-hit kill
                        scaryChars[i].health = 0.0f;
                        scaryChars[i].isDead = true;
                    }
                }
            }
            
            // update the scary characters to follow the player
            if (gameState == GAME_STATE_PLAYING) {
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                
                // update each scary character
                for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                    // Skip dead enemies
                    if (scaryChars[i].isDead) continue;
                    Vector2 charPos = (Vector2){scaryChars[i].position.x, scaryChars[i].position.z};
                    
                    // calculate the direction to the player
                    Vector2 dir = (Vector2){
                        playerPos2D.x - charPos.x,
                        playerPos2D.y - charPos.y
                    };
                    
                    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if (dist > 0.001f) {
                        dir.x /= dist;
                        dir.y /= dist;
                        
                        float randomAngle = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159265359f * SCARY_CHAR_RANDOMNESS;
                        float cosAngle = cosf(randomAngle);
                        float sinAngle = sinf(randomAngle);
                        Vector2 randomDir = (Vector2){
                            dir.x * cosAngle - dir.y * sinAngle,
                            dir.x * sinAngle + dir.y * cosAngle
                        };
                        dir.x = dir.x * (1.0f - SCARY_CHAR_RANDOMNESS) + randomDir.x * SCARY_CHAR_RANDOMNESS;
                        dir.y = dir.y * (1.0f - SCARY_CHAR_RANDOMNESS) + randomDir.y * SCARY_CHAR_RANDOMNESS;

                        float dirLen = sqrtf(dir.x * dir.x + dir.y * dir.y);
                        if (dirLen > 0.001f) {
                            dir.x /= dirLen;
                            dir.y /= dirLen;
                        }

                        Vector2 moveStep = (Vector2){
                            dir.x * scaryChars[i].speed * dt,
                            dir.y * scaryChars[i].speed * dt
                        };
                        
                        Vector2 testX = (Vector2){charPos.x + moveStep.x, charPos.y};
                        if (!CollidesAny(testX, scaryChars[i].radius, walls, wallCount)) {
                            charPos.x = testX.x;
                        }
                        
                        Vector2 testZ = (Vector2){charPos.x, charPos.y + moveStep.y};
                        if (!CollidesAny(testZ, scaryChars[i].radius, walls, wallCount)) {
                            charPos.y = testZ.y;
                        }
                        
                        scaryChars[i].position.x = charPos.x;
                        scaryChars[i].position.z = charPos.y;
                    }
                    
                    // check collision with the player (only if enemy is alive)
                    if (!scaryChars[i].isDead && CircleCircleIntersect(playerPos2D, PLAYER_RADIUS, charPos, scaryChars[i].radius)) {
                        // Play jumpscare sound
                        if (assets && assets->jumpScareSound.frameCount > 0) {
                            PlaySound(assets->jumpScareSound);
                        }
                        gameState = GAME_STATE_GAMEOVER;
                        break;
                    }
                }
            }
            
            // Check for battery pickup collisions
            if (batteries && batteryCount > 0) {
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                const float BATTERY_PICKUP_RADIUS = 0.8f; // Pickup radius
                
                for (int i = 0; i < batteryCount; i++) {
                    if (!batteries[i].collected) {
                        Vector2 batteryPos2D = (Vector2){batteries[i].position.x, batteries[i].position.z};
                        
                        // Check if player is close enough to collect
                        if (CircleCircleIntersect(playerPos2D, PLAYER_RADIUS, batteryPos2D, BATTERY_PICKUP_RADIUS)) {
                            // Collect the battery
                            batteries[i].collected = true;
                            
                            // Recharge flashlight (clamp to max)
                            flashlightBattery += BATTERY_RECHARGE_AMOUNT;
                            if (flashlightBattery > FLASHLIGHT_MAX_BATTERY) {
                                flashlightBattery = FLASHLIGHT_MAX_BATTERY;
                            }
                            
                            // Play pickup sound
                            if (assets && assets->batteryPickupSound.frameCount > 0) {
                                PlaySound(assets->batteryPickupSound);
                            }
                        }
                    }
                }
            }
            
            // Check for weapon pickup collisions
            if (weapons && weaponCount > 0) {
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                const float WEAPON_PICKUP_RADIUS = 0.8f;
                
                for (int i = 0; i < weaponCount; i++) {
                    if (!weapons[i].collected) {
                        Vector2 weaponPos2D = (Vector2){weapons[i].position.x, weapons[i].position.z};
                        
                        // Check if player is close enough to collect
                        if (CircleCircleIntersect(playerPos2D, PLAYER_RADIUS, weaponPos2D, WEAPON_PICKUP_RADIUS)) {
                            // Collect the weapon
                            weapons[i].collected = true;
                            
                            // Equip the weapon (if not already equipped)
                            if (playerWeapon.type == WEAPON_NONE) {
                                playerWeapon = GetWeaponStats(weapons[i].type);
                            }
                            // Add bullets when picking up guns
                            bulletCount += 1; // Each gun pickup gives 1 bullet
                            
                            // Play pickup sound (reuse battery sound for now)
                            if (assets && assets->batteryPickupSound.frameCount > 0) {
                                PlaySound(assets->batteryPickupSound);
                            }
                        }
                    }
                }
            }
            
            // check if the player reached the exit
            int cellX, cellY;
            Maze_WorldToCell(maze, playerPos.x, playerPos.z, &cellX, &cellY);
            if (Maze_IsExit(maze, cellX, cellY)) {
                // Only play victory sound once when first winning
                if (gameState != GAME_STATE_WON && assets && assets->victorySound.frameCount > 0) {
                    PlaySound(assets->victorySound);
                }
                gameState = GAME_STATE_WON;
                // update the best record if this is better
                if (bestRecord < 0.0f || gameTimer < bestRecord) {
                    bestRecord = gameTimer;
                    SaveBestRecord(bestRecord);
                }
            }
        }

        // update the camera
        if (gameInitialized) {
            cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
            cam.target = (Vector3){
                cam.position.x + forward.x,
                cam.position.y + forward.y,
                cam.position.z + forward.z
            };
        }
        
        // start drawing
        BeginDrawing();
        // Much darker background for horror atmosphere (5% lighter)
        ClearBackground((Color){3, 2, 4, 255});
        
        // Render menu or game
        if (gameState == GAME_STATE_MENU) {
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();
            
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 240});

            const char* titleText = "3D MAZE HORROR";
            int titleSize = 72;
            int titleWidth = MeasureText(titleText, titleSize);
            
            // Title shadow
            DrawText(titleText, (screenWidth - titleWidth) / 2 + 4, screenHeight / 2 - 200 + 4, titleSize, (Color){40, 0, 0, 255});
            // Title glow
            DrawText(titleText, (screenWidth - titleWidth) / 2, screenHeight / 2 - 200, titleSize, (Color){180, 0, 0, 255});
            
            // Subtitle
            const char* subtitleText = "Escape the darkness...";
            int subtitleSize = 28;
            int subtitleWidth = MeasureText(subtitleText, subtitleSize);
            DrawText(subtitleText, (screenWidth - subtitleWidth) / 2, screenHeight / 2 - 120, subtitleSize, (Color){150, 150, 150, 255});
            
            // Start game instruction with blinking effect
            float time = GetTime();
            bool showPrompt = ((int)(time * 2)) % 2 == 0; // Blinking each 0.5 seconds
            
            if (showPrompt) {
                const char* startText = "Press ENTER or SPACE to Start";
                int startSize = 32;
                int startWidth = MeasureText(startText, startSize);
                DrawText(startText, (screenWidth - startWidth) / 2, screenHeight / 2 + 40, startSize, (Color){220, 220, 220, 255});
            }
            
            // Controls
            const char* controlsText = "Controls: WASD - Move | Shift - Run | Space - Jump | F - Toggle Mouse | T - Flashlight | Left/Right Click - Shoot | ESC - Quit";
            int controlsSize = 18;
            int controlsWidth = MeasureText(controlsText, controlsSize);
            DrawText(controlsText, (screenWidth - controlsWidth) / 2, screenHeight - 80, controlsSize, (Color){120, 120, 120, 255});
            
            // Best record display (if exists)
            if (bestRecord >= 0.0f) {
                char bestText[128];
                int bestMinutes = (int)(bestRecord / 60.0f);
                int bestSeconds = (int)bestRecord % 60;
                int bestMilliseconds = (int)((bestRecord - (int)bestRecord) * 100.0f);
                snprintf(bestText, sizeof(bestText), "Best Time: %02d:%02d.%02d", bestMinutes, bestSeconds, bestMilliseconds);
                int bestSize = 24;
                int bestWidth = MeasureText(bestText, bestSize);
                DrawText(bestText, (screenWidth - bestWidth) / 2, screenHeight / 2 + 100, bestSize, (Color){200, 150, 0, 255});
            }
        } else if (gameInitialized) {
            BeginMode3D(cam);

        // render the maze with textures
        if (maze) {
            RenderMaze(maze, assets, torches, torchCount, cam.position, forward, flashlightOn, flashlightBattery);
        }
        
        if (torches && torchCount > 0) {
            Torches_Render(torches, torchCount);
            
            for (int i = 0; i < torchCount; i++) {
                // Subtle, minimal flickering
                float baseFlicker = 0.92f + 0.08f * sinf(torches[i].flickerTime);
                
                float intensity = torches[i].baseIntensity * baseFlicker;
                
                Vector3 lightPos = torches[i].position;
                lightPos.y += 0.3f;
                
                // More red/orange horror lighting
                float redIntensity = intensity * 1.2f;
                float greenIntensity = intensity * 0.6f;
                float blueIntensity = intensity * 0.2f;
                
                // Clamp to valid color range
                if (redIntensity > 1.0f) redIntensity = 1.0f;
                if (greenIntensity > 1.0f) greenIntensity = 1.0f;
                if (blueIntensity > 1.0f) blueIntensity = 1.0f;
                
                // Core light glow (brighter, more visible)
                Color lightColor = (Color){
                    (unsigned char)(255 * redIntensity),
                    (unsigned char)(180 * greenIntensity),
                    (unsigned char)(60 * blueIntensity),
                    255
                };
                float lightSize = 0.08f * intensity;
                DrawCube(lightPos, lightSize, lightSize, lightSize, lightColor);
                
                // Volumetric light sphere (smaller, subtle)
                float volumetricSize = 1.2f * intensity;
                float volumetricAlpha = intensity * 0.25f;
                if (volumetricAlpha > 0.25f) volumetricAlpha = 0.25f;
                Color volumetricColor = (Color){
                    (unsigned char)(220 * redIntensity),
                    (unsigned char)(120 * greenIntensity),
                    (unsigned char)(40 * blueIntensity),
                    (unsigned char)(volumetricAlpha * 255)
                };
                // Draw multiple layers for volumetric effect with better falloff
                DrawSphere(lightPos, volumetricSize * 0.8f, volumetricColor);
                DrawSphere(lightPos, volumetricSize * 0.6f, volumetricColor);
                DrawSphere(lightPos, volumetricSize * 0.4f, volumetricColor);
                DrawSphere(lightPos, volumetricSize * 0.2f, volumetricColor);
                
                // Light pool on the floor (smaller, subtle)
                Vector3 floorLightPos = (Vector3){lightPos.x, 0.05f, lightPos.z};
                float poolRadius = 1.0f * intensity;
                float poolAlpha = intensity * 0.4f;
                if (poolAlpha > 0.4f) poolAlpha = 0.4f;
                
                // Draw light pool as a colored plane on the floor
                Color poolColor = (Color){
                    (unsigned char)(180 * redIntensity),
                    (unsigned char)(100 * greenIntensity),
                    (unsigned char)(30 * blueIntensity),
                    (unsigned char)(poolAlpha * 255)
                };
                
                // Draw multiple concentric circles for smooth falloff
                for (int layer = 0; layer < 5; layer++) {
                    float layerRadius = poolRadius * (1.0f - layer * 0.2f);
                    float layerAlpha = poolAlpha * (1.0f - layer * 0.25f);
                    if (layerRadius > 0.1f) {
                        Color layerColor = poolColor;
                        layerColor.a = (unsigned char)(layerAlpha * 255);
                        DrawPlane(floorLightPos, (Vector2){layerRadius * 2.0f, layerRadius * 2.0f}, layerColor);
                    }
                }
            }
            
            // Render darkening overlay in areas far from torches
            // This creates dramatic contrast between lit and unlit areas
            if (maze) {
                float mazeWidth = maze->width * maze->cellSize;
                float mazeHeight = maze->height * maze->cellSize;
                
                // Sample points on the floor to determine darkness
                int samplesX = 20;
                int samplesZ = 20;
                float sampleSpacingX = mazeWidth / (float)samplesX;
                float sampleSpacingZ = mazeHeight / (float)samplesZ;
                
                for (int sx = 0; sx < samplesX; sx++) {
                    for (int sz = 0; sz < samplesZ; sz++) {
                        float sampleX = -mazeWidth * 0.5f + sx * sampleSpacingX + sampleSpacingX * 0.5f;
                        float sampleZ = -mazeHeight * 0.5f + sz * sampleSpacingZ + sampleSpacingZ * 0.5f;
                        Vector3 samplePos = (Vector3){sampleX, 0.0f, sampleZ};
                        
                        // Find minimum distance to any torch
                        float minDist = 1000.0f;
                        for (int i = 0; i < torchCount; i++) {
                            float dx = samplePos.x - torches[i].position.x;
                            float dz = samplePos.z - torches[i].position.z;
                            float dist = sqrtf(dx*dx + dz*dz);
                            if (dist < minDist) minDist = dist;
                        }
                        
                        // Darken areas far from torches
                        float lightInfluence = 1.0f - clampf((minDist - 1.0f) / 3.0f, 0.0f, 1.0f);
                        float darkness = 1.0f - lightInfluence;
                        
                        if (darkness > 0.1f) {
                            // Draw darkening overlay
                            float overlayAlpha = darkness * 0.6f;
                            Color darkColor = (Color){0, 0, 0, (unsigned char)(overlayAlpha * 255)};
                            DrawPlane((Vector3){samplePos.x, 0.02f, samplePos.z}, 
                                     (Vector2){sampleSpacingX * 1.2f, sampleSpacingZ * 1.2f}, 
                                     darkColor);
                        }
                    }
                }
            }
            
            // render the particle systems (flames)
            if (particleSystems) {
                for (int i = 0; i < torchCount; i++) {
                    if (particleSystems[i]) {
                        ParticleSystem_Render(particleSystems[i]);
                    }
                }
            }
        }
        
        // Render battery pickups
        if (gameInitialized && batteries && batteryCount > 0) {
            float globalTime = GetTime();
            Batteries_Render(batteries, batteryCount, globalTime);
        }
        
        // Render weapon pickups
        if (gameInitialized && weapons && weaponCount > 0) {
            float globalTime = GetTime();
            Weapons_Render(weapons, weaponCount, globalTime);
        }
        
        // Render flashlight dust particles (volumetric effect)
        if (gameInitialized && flashlightDust && flashlightOn && gameState == GAME_STATE_PLAYING) {
            Vector3 flashlightPos = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
            Vector3 flashlightDir = forward;
            FlashlightDust_Render(flashlightDust, flashlightPos, flashlightDir);
        }
        
        // render the scary characters (dark, menacing figures)
        if (gameState == GAME_STATE_PLAYING || gameState == GAME_STATE_GAMEOVER) {
            float globalTime = GetTime();
            
            for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                // Skip dead enemies
                if (scaryChars[i].isDead) continue;
                
                Vector3 charRenderPos = scaryChars[i].position;
                charRenderPos.y = scaryChars[i].height * 0.5f;
                
                // Check distance to nearest torch for horror lighting effect
                float minTorchDist = 1000.0f;
                if (torches && torchCount > 0) {
                    for (int j = 0; j < torchCount; j++) {
                        float dx = charRenderPos.x - torches[j].position.x;
                        float dy = charRenderPos.y - torches[j].position.y;
                        float dz = charRenderPos.z - torches[j].position.z;
                        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                        if (dist < minTorchDist) minTorchDist = dist;
                    }
                }
                
                // Darker when far from torches, slightly lit when near (horror effect)
                float torchInfluence = 1.0f - clampf(minTorchDist / 3.0f, 0.0f, 1.0f);
                float baseDarkness = 0.1f + torchInfluence * 0.15f;
                
                // draw a dark, scary character (dark red/black cube with slight glow)
                Color scaryColor = (Color){
                    (unsigned char)((40 + i * 5) * baseDarkness), 
                    0, 
                    (unsigned char)((i * 3) * baseDarkness), 
                    255
                };
                DrawCube(charRenderPos, scaryChars[i].radius * 2.0f, scaryChars[i].height, scaryChars[i].radius * 2.0f, scaryColor);
                
                // add a subtle dark glow around it (pulsing for horror)
                float pulse = 0.8f + 0.2f * sinf(globalTime * 2.0f + i);
                DrawCubeWires(charRenderPos, scaryChars[i].radius * 2.2f, scaryChars[i].height * 1.1f, scaryChars[i].radius * 2.2f, 
                             (Color){(unsigned char)(80 * pulse), 0, 0, (unsigned char)(100 * pulse)});
            }
        }
        
        // Flashlight lighting is handled in the lighting system (no visual cubes)
        
        // Render first-person weapon in 3D
        if (gameInitialized && gameState == GAME_STATE_PLAYING && playerWeapon.type != WEAPON_NONE) {
            // Calculate weapon position relative to camera (in front, slightly right and down)
            // Position it much closer for first-person view
            Vector3 weaponBasePos = (Vector3){
                cam.position.x + right.x * 0.2f - forward.x * 0.2f,  // Right and forward (close)
                cam.position.y - 0.1f,  // Slightly down
                cam.position.z + right.z * 0.2f - forward.z * 0.2f
            };
            
            // Attack animation (recoil effect)
            float recoilOffset = 0.0f;
            if (isAttacking) {
                recoilOffset = sinf((0.3f - attackAnimationTime) / 0.3f * PI) * 0.08f;
                weaponBasePos.x -= forward.x * recoilOffset;
                weaponBasePos.y -= forward.y * recoilOffset;
                weaponBasePos.z -= forward.z * recoilOffset;
            }
            
            // Draw 3D gun model (properly sized for first-person view)
            if (playerWeapon.type == WEAPON_GUN) {
                Color bodyColor = (Color){60, 60, 70, 255}; // Dark gray/black for gun body
                Color barrelColor = (Color){40, 40, 50, 255}; // Darker for barrel
                Color gripColor = (Color){80, 50, 30, 255}; // Brown for grip
                
                // Gun body (main horizontal part) - smaller, realistic size
                Vector3 bodyPos = weaponBasePos;
                bodyPos.x += forward.x * 0.08f;
                bodyPos.y += forward.y * 0.01f;
                bodyPos.z += forward.z * 0.08f;
                DrawCube(bodyPos, 0.25f, 0.06f, 0.08f, bodyColor);
                
                // Gun barrel (extending forward along camera direction)
                Vector3 barrelPos = weaponBasePos;
                barrelPos.x += forward.x * 0.22f;
                barrelPos.y += forward.y * 0.01f;
                barrelPos.z += forward.z * 0.22f;
                DrawCube(barrelPos, 0.15f, 0.05f, 0.06f, barrelColor);
                
                // Gun grip (below and behind)
                Vector3 gripPos = weaponBasePos;
                gripPos.x -= forward.x * 0.05f;
                gripPos.y -= 0.1f;
                gripPos.z -= forward.z * 0.05f;
                DrawCube(gripPos, 0.08f, 0.12f, 0.06f, gripColor);
                
                // Gun trigger guard (wireframe, around trigger area)
                Vector3 guardPos = weaponBasePos;
                guardPos.x += forward.x * 0.02f;
                guardPos.y -= 0.06f;
                guardPos.z += forward.z * 0.02f;
                DrawCubeWires(guardPos, 0.12f, 0.08f, 0.08f, bodyColor);
            }
        }
        
            EndMode3D();
        }
        
        // draw the crosshair
        if (gameInitialized && gameState == GAME_STATE_PLAYING) {
            int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
            DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
            DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);
        }
        
        // draw the HUD
        if (gameInitialized && gameState == GAME_STATE_PLAYING) {
            DrawText("WASD: move | Shift: run | Space: jump | F: toggle mouse | T: flashlight | Left/Right Click: shoot | R: restart | Esc: quit",
                     20, 20, 18, RAYWHITE);
            
            // Display timer
            char timerText[64];
            int minutes = (int)(gameTimer / 60.0f);
            int seconds = (int)gameTimer % 60;
            int milliseconds = (int)((gameTimer - (int)gameTimer) * 100.0f);
            snprintf(timerText, sizeof(timerText), "Time: %02d:%02d.%02d", minutes, seconds, milliseconds);
            DrawText(timerText, 20, 50, 24, YELLOW);
            
            // Display flashlight battery
            int screenWidth = GetScreenWidth();
            int batteryX = screenWidth - 250;
            int batteryY = 20;
            int batteryWidth = 220;  // Made much longer
            int batteryHeight = 40;
            
            // Battery outline (with small tab on right)
            DrawRectangleLines(batteryX, batteryY, batteryWidth, batteryHeight, WHITE);
            DrawRectangle(batteryX + batteryWidth, batteryY + 10, 5, 20, WHITE);  // Battery tab
            
            // Battery fill (color changes based on level)
            int fillWidth = (int)((batteryWidth - 4) * (flashlightBattery / FLASHLIGHT_MAX_BATTERY));
            Color batteryColor;
            if (flashlightBattery > 50.0f) {
                batteryColor = GREEN;
            } else if (flashlightBattery > 20.0f) {
                batteryColor = YELLOW;
            } else {
                batteryColor = RED;
            }
            
            if (fillWidth > 0) {
                DrawRectangle(batteryX + 2, batteryY + 2, fillWidth, batteryHeight - 4, batteryColor);
            }
            
            // Battery percentage text
            char batteryText[32];
            snprintf(batteryText, sizeof(batteryText), "%.0f%%", flashlightBattery);
            int textWidth = MeasureText(batteryText, 20);
            DrawText(batteryText, batteryX + (batteryWidth - textWidth) / 2, batteryY + 10, 20, WHITE);
            
            // Flashlight status
            const char* flashStatus = flashlightOn ? "ON" : "OFF";
            DrawText(flashStatus, batteryX, batteryY - 20, 16, flashlightOn ? GREEN : GRAY);
            
            // Weapon status
            if (playerWeapon.type != WEAPON_NONE) {
                char weaponText[64];
                snprintf(weaponText, sizeof(weaponText), "Weapon: Gun | Bullets: %d", bulletCount);
                DrawText(weaponText, 20, 80, 20, YELLOW);
                
                // Attack/shoot status
                if (bulletCount <= 0) {
                    DrawText("No Bullets - Find More Guns!", 20, 105, 16, RED);
                } else {
                    DrawText("Left/Right Click: Shoot", 20, 105, 16, GREEN);
                }
            } else {
                DrawText("No Gun - Find one to fight!", 20, 80, 20, RED);
            }
        }
        
        // Win and game over screens (only when game is initialized)
        if (gameInitialized && gameState == GAME_STATE_WON) {
            // Win screen
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();
            
            // Semi-transparent overlay
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 200});
            
            // Win message
            const char* winText = "YOU WIN!";
            int fontSize = 60;
            int textWidth = MeasureText(winText, fontSize);
            DrawText(winText, (screenWidth - textWidth) / 2, screenHeight / 2 - 120, fontSize, GREEN);
            
            // Display completion time
            char timeText[128];
            int minutes = (int)(gameTimer / 60.0f);
            int seconds = (int)gameTimer % 60;
            int milliseconds = (int)((gameTimer - (int)gameTimer) * 100.0f);
            snprintf(timeText, sizeof(timeText), "Time: %02d:%02d.%02d", minutes, seconds, milliseconds);
            fontSize = 32;
            textWidth = MeasureText(timeText, fontSize);
            DrawText(timeText, (screenWidth - textWidth) / 2, screenHeight / 2 - 40, fontSize, YELLOW);
            
            // Display best record
            char bestText[128];
            if (bestRecord >= 0.0f) {
                int bestMinutes = (int)(bestRecord / 60.0f);
                int bestSeconds = (int)bestRecord % 60;
                int bestMilliseconds = (int)((bestRecord - (int)bestRecord) * 100.0f);
                snprintf(bestText, sizeof(bestText), "Best Record: %02d:%02d.%02d", bestMinutes, bestSeconds, bestMilliseconds);
            } else {
                snprintf(bestText, sizeof(bestText), "Best Record: --:--.--");
            }
            fontSize = 28;
            textWidth = MeasureText(bestText, fontSize);
            DrawText(bestText, (screenWidth - textWidth) / 2, screenHeight / 2 + 10, fontSize, GOLD);
            
            // Restart instruction
            const char* restartText = "Press R to restart or Esc to quit";
            fontSize = 24;
            textWidth = MeasureText(restartText, fontSize);
            DrawText(restartText, (screenWidth - textWidth) / 2, screenHeight / 2 + 60, fontSize, RAYWHITE);
        } else if (gameInitialized && gameState == GAME_STATE_GAMEOVER) {
            // Game over screen
            int screenWidth = GetScreenWidth();
            int screenHeight = GetScreenHeight();
            
            // Dark red overlay for scary effect
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){40, 0, 0, 220});
            
            // Game over message
            const char* gameOverText = "GAME OVER";
            int fontSize = 60;
            int textWidth = MeasureText(gameOverText, fontSize);
            DrawText(gameOverText, (screenWidth - textWidth) / 2, screenHeight / 2 - 60, fontSize, RED);
            
            // Scary message
            const char* scaryText = "You were caught...";
            fontSize = 28;
            textWidth = MeasureText(scaryText, fontSize);
            DrawText(scaryText, (screenWidth - textWidth) / 2, screenHeight / 2, fontSize, (Color){200, 0, 0, 255});
            
            // Restart instruction
            const char* restartText = "Press R to restart or Esc to quit";
            fontSize = 24;
            textWidth = MeasureText(restartText, fontSize);
            DrawText(restartText, (screenWidth - textWidth) / 2, screenHeight / 2 + 40, fontSize, RAYWHITE);
        }
        
        EndDrawing();
    }
    
    // Cleanup
    if (maze) Maze_Destroy(maze);
    if (walls) free(walls);
    if (torches) free(torches);
    if (batteries) free(batteries);
    if (particleSystems) {
        for (int i = 0; i < torchCount; i++) {
            if (particleSystems[i]) {
                ParticleSystem_Destroy(particleSystems[i]);
            }
        }
        free(particleSystems);
    }
    if (assets) Assets_Unload(assets);
    if (flashlightDust) FlashlightDust_Destroy(flashlightDust);
    if (weapons) free(weapons);
    
    // Cleanup static models
    CleanupCubeModel();
    CleanupPlaneModel();
    
    // Close audio device
    CloseAudioDevice();
    
    CloseWindow();
    return 0;
}
