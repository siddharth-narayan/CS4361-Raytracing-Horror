#include "raylib.h"
#include "../include/maze.h"
#include "../include/assets.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Game Constants
#define MAZE_WIDTH           15      // Number of cells horizontally
#define MAZE_HEIGHT          15      // Number of cells vertically
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

#define SCARY_CHAR_COUNT     3       // Number of scary characters
#define SCARY_CHAR_SPEED     2.8f    // Scary character movement speed
#define SCARY_CHAR_RADIUS    0.35f   // Collision radius
#define SCARY_CHAR_HEIGHT    2.2f    // Height of scary character
#define SCARY_CHAR_RANDOMNESS 0.15f  // Randomness factor in movement

#define BEST_RECORD_FILE     "best_record.txt"

// Game state
typedef enum {
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
} ScaryCharacter;

// Helper function: Clamp float value
static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
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
                     ScaryCharacter* scaryChars, int scaryCharCount, float* gameTimer) {
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

// Render the maze in 3D
static void RenderMaze(const Maze* maze, const GameAssets* assets) {
    if (!maze || !assets || !assets->loaded) return;
    
    const float halfCell = maze->cellSize * 0.5f;
    const float wallHalfHeight = WALL_HEIGHT * 0.5f;
    
    float mazeWidth = maze->width * maze->cellSize;
    float mazeHeight = maze->height * maze->cellSize;
    
    // Draw the floor
    DrawTexturedPlane((Vector3){0, 0, 0}, (Vector2){mazeWidth, mazeHeight}, assets->floorTexture);
    
    // Draw the ceiling
    DrawTexturedPlane((Vector3){0, WALL_HEIGHT, 0}, (Vector2){mazeWidth, mazeHeight}, assets->ceilingTexture);
    
    // Draw the walls for each cell
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            
            // Draw the north wall
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                DrawTexturedCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ - halfCell
                }, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
            }
            
            // Draw the south wall
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                DrawTexturedCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ + halfCell
                }, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
            }
            
            // Draw the west wall
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                DrawTexturedCube((Vector3){
                    worldX - halfCell,
                    wallHalfHeight,
                    worldZ
                }, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
            }
            
            // Draw the east wall
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                DrawTexturedCube((Vector3){
                    worldX + halfCell,
                    wallHalfHeight,
                    worldZ
                }, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
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
    
    // Set up the window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "3D Maze Game | WASD+mouse, Shift run, Space jump, F toggle mouse, R restart");
    SetTargetFPS(120);
    
    bool mouseCaptured = true;
    DisableCursor();
    
    Maze* maze = NULL;
    WallRect* walls = NULL;
    int wallCount = 0;
    GameState gameState = GAME_STATE_PLAYING;
    
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
    
    // Set up the scary characters
    ScaryCharacter scaryChars[SCARY_CHAR_COUNT] = {0};
    
    // Set up the timer and best record
    float gameTimer = 0.0f;
    float bestRecord = LoadBestRecord();
    
    // Initialize the game
    InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
             &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT, &gameTimer);
    
    // Start the main game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Toggle the mouse capture
        if (IsKeyPressed(KEY_F)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) DisableCursor();
            else EnableCursor();
        }
        
        // Restart the game
        if (IsKeyPressed(KEY_R)) {
            InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
                     &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT, &gameTimer);
        }
        // timer update
        if (gameState == GAME_STATE_PLAYING) {
            gameTimer += dt;
        }

        if (torches && torchCount > 0) {
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
        
        // mouse look
        if (mouseCaptured && gameState == GAME_STATE_PLAYING) {
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
        
        // player movement input
        if (gameState == GAME_STATE_PLAYING) {
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
            
            // update the scary characters to follow the player
            if (gameState == GAME_STATE_PLAYING) {
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                
                // update each scary character
                for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
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
                    
                    // check collision with the player
                    if (CircleCircleIntersect(playerPos2D, PLAYER_RADIUS, charPos, scaryChars[i].radius)) {
                        gameState = GAME_STATE_GAMEOVER;
                        break;
                    }
                }
            }
            
            // check if the player reached the exit
            int cellX, cellY;
            Maze_WorldToCell(maze, playerPos.x, playerPos.z, &cellX, &cellY);
            if (Maze_IsExit(maze, cellX, cellY)) {
                gameState = GAME_STATE_WON;
                // update the best record if this is better
                if (bestRecord < 0.0f || gameTimer < bestRecord) {
                    bestRecord = gameTimer;
                    SaveBestRecord(bestRecord);
                }
            }
        }

        // update the camera
        cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
        cam.target = (Vector3){
            cam.position.x + forward.x,
            cam.position.y + forward.y,
            cam.position.z + forward.z
        };
        
        // start drawing
        BeginDrawing();
        ClearBackground((Color){5, 5, 8, 255});
        
        BeginMode3D(cam);

        // render the maze with textures
        if (maze) {
            RenderMaze(maze, assets);
        }
        
        if (torches && torchCount > 0) {
            Torches_Render(torches, torchCount);
            
            for (int i = 0; i < torchCount; i++) {
                float flicker = 0.5f + 0.4f * sinf(torches[i].flickerTime) + 
                                0.15f * sinf(torches[i].flickerTime * 3.5f) +
                                0.1f * sinf(torches[i].flickerTime * 7.0f);
                if ((int)(torches[i].flickerTime * 10) % 23 == 0) {
                    flicker *= 0.3f;
                }
                float intensity = torches[i].baseIntensity * flicker;
                
                Vector3 lightPos = torches[i].position;
                lightPos.y += 0.3f;
                
                // draw the light glow
                Color lightColor = (Color){
                    (unsigned char)(220 * intensity),
                    (unsigned char)(150 * intensity),
                    (unsigned char)(80 * intensity),
                    255
                };
                float lightSize = 0.12f * intensity;
                DrawCube(lightPos, lightSize, lightSize, lightSize, lightColor);
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
        
        // render the scary characters (dark, menacing figures)
        if (gameState == GAME_STATE_PLAYING || gameState == GAME_STATE_GAMEOVER) {
            for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                Vector3 charRenderPos = scaryChars[i].position;
                charRenderPos.y = scaryChars[i].height * 0.5f;
                
                // draw a dark, scary character (dark red/black cube with slight glow)
                Color scaryColor = (Color){
                    (unsigned char)(40 + i * 5), 
                    0, 
                    (unsigned char)(i * 3), 
                    255
                };
                DrawCube(charRenderPos, scaryChars[i].radius * 2.0f, scaryChars[i].height, scaryChars[i].radius * 2.0f, scaryColor);
                
                // add a subtle dark glow around it
                DrawCubeWires(charRenderPos, scaryChars[i].radius * 2.2f, scaryChars[i].height * 1.1f, scaryChars[i].radius * 2.2f, (Color){80, 0, 0, 100});
            }
        }
        
        EndMode3D();
        
        // draw the crosshair
        if (gameState == GAME_STATE_PLAYING) {
            int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
            DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
            DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);
        }
        
        // draw the HUD
        if (gameState == GAME_STATE_PLAYING) {
            DrawText("WASD: move | Shift: run | Space: jump | F: toggle mouse | R: restart | Esc: quit",
                     20, 20, 18, RAYWHITE);
            
            // Display timer
            char timerText[64];
            int minutes = (int)(gameTimer / 60.0f);
            int seconds = (int)gameTimer % 60;
            int milliseconds = (int)((gameTimer - (int)gameTimer) * 100.0f);
            snprintf(timerText, sizeof(timerText), "Time: %02d:%02d.%02d", minutes, seconds, milliseconds);
            DrawText(timerText, 20, 50, 24, YELLOW);
        } else if (gameState == GAME_STATE_WON) {
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
        } else if (gameState == GAME_STATE_GAMEOVER) {
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
    if (particleSystems) {
        for (int i = 0; i < torchCount; i++) {
            if (particleSystems[i]) {
                ParticleSystem_Destroy(particleSystems[i]);
            }
        }
        free(particleSystems);
    }
    if (assets) Assets_Unload(assets);
    
    // Cleanup static models
    CleanupCubeModel();
    CleanupPlaneModel();
    
    CloseWindow();
    return 0;
}
