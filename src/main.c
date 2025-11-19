#include "raylib.h"
#include "../include/maze.h"
#include "../include/assets.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

// ---------- Game Constants ----------
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
#define SCARY_CHAR_SPEED     3.5f    // Scary character movement speed (slightly slower than player)
#define SCARY_CHAR_RADIUS    0.35f   // Collision radius
#define SCARY_CHAR_HEIGHT    2.2f    // Height of scary character

// Game state
typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_WON,
    GAME_STATE_GAMEOVER
} GameState;

// Scary character structure
typedef struct {
    Vector3 position;      // XZ position (Y will be 0 for ground level)
    float speed;          // Movement speed
    float radius;         // Collision radius
    float height;         // Height of the character
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

// Initialize game (generate maze, reset player)
static void InitGame(Maze** maze, WallRect** walls, int* wallCount, Vector3* playerPos, 
                     float* yaw, float* pitch, GameState* gameState,
                     Torch** torches, int* torchCount, ParticleSystem*** particleSystems,
                     ScaryCharacter* scaryChars, int scaryCharCount) {
    // Free old maze if exists
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
    
    // Create and generate new maze
    *maze = Maze_Create(MAZE_WIDTH, MAZE_HEIGHT, CELL_SIZE);
    if (!*maze) {
        TraceLog(LOG_ERROR, "Failed to create maze!");
        return;
    }
    
    Maze_Generate(*maze);
    
    // Allocate wall rectangles
    int maxWalls = MAZE_WIDTH * MAZE_HEIGHT * 4; // Maximum possible walls
    *walls = (WallRect*)malloc(maxWalls * sizeof(WallRect));
    if (!*walls) {
        TraceLog(LOG_ERROR, "Failed to allocate wall rectangles!");
        return;
    }
    
    *wallCount = Maze_GetWallRects(*maze, *walls, maxWalls);
    
    // Generate torches (sparse random placement for scary atmosphere)
    int maxTorches = 25; // Very few torches for dark, scary atmosphere
    *torchCount = Torches_Generate(*maze, torches, maxTorches);
    
    // Create particle systems for each torch
    if (*torchCount > 0) {
        *particleSystems = (ParticleSystem**)malloc(*torchCount * sizeof(ParticleSystem*));
        if (*particleSystems) {
            for (int i = 0; i < *torchCount; i++) {
                (*particleSystems)[i] = ParticleSystem_Create(20); // Reduced to 20 particles per torch for performance
            }
        }
    }
    
    // Reset player to start position
    Vector2 startWorld = Maze_CellToWorld(*maze, (int)(*maze)->startPos.x, (int)(*maze)->startPos.y);
    playerPos->x = startWorld.x;
    playerPos->y = 0.0f;
    playerPos->z = startWorld.y;
    
    // Initialize scary characters at different positions (spread out from exit and corners)
    if (scaryChars && scaryCharCount > 0) {
        // First character at exit
        Vector2 exitWorld = Maze_CellToWorld(*maze, (int)(*maze)->exitPos.x, (int)(*maze)->exitPos.y);
        scaryChars[0].position = (Vector3){exitWorld.x, 0.0f, exitWorld.y};
        scaryChars[0].speed = SCARY_CHAR_SPEED;
        scaryChars[0].radius = SCARY_CHAR_RADIUS;
        scaryChars[0].height = SCARY_CHAR_HEIGHT;
        
        // Second character at top-right corner (if exists)
        if (scaryCharCount > 1) {
            int cornerX = (*maze)->width - 1;
            int cornerY = 0;
            Vector2 cornerWorld = Maze_CellToWorld(*maze, cornerX, cornerY);
            scaryChars[1].position = (Vector3){cornerWorld.x, 0.0f, cornerWorld.y};
            scaryChars[1].speed = SCARY_CHAR_SPEED;
            scaryChars[1].radius = SCARY_CHAR_RADIUS;
            scaryChars[1].height = SCARY_CHAR_HEIGHT;
        }
        
        // Third character at bottom-left corner (if exists)
        if (scaryCharCount > 2) {
            int cornerX = 0;
            int cornerY = (*maze)->height - 1;
            Vector2 cornerWorld = Maze_CellToWorld(*maze, cornerX, cornerY);
            scaryChars[2].position = (Vector3){cornerWorld.x, 0.0f, cornerWorld.y};
            scaryChars[2].speed = SCARY_CHAR_SPEED;
            scaryChars[2].radius = SCARY_CHAR_RADIUS;
            scaryChars[2].height = SCARY_CHAR_HEIGHT;
        }
    }
    
    *yaw = 0.0f;
    *pitch = 0.0f;
    *gameState = GAME_STATE_PLAYING;
}

// Static cube model for textured rendering
static Model s_cubeModel = {0};
static bool s_cubeModelCreated = false;

// Helper: Get or create cube model
static Model* GetCubeModel(void) {
    if (!s_cubeModelCreated) {
        Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        s_cubeModel = LoadModelFromMesh(cubeMesh);
        s_cubeModelCreated = true;
    }
    return &s_cubeModel;
}

// Cleanup cube model
static void CleanupCubeModel(void) {
    if (s_cubeModelCreated && s_cubeModel.meshCount > 0) {
        UnloadModel(s_cubeModel);
        s_cubeModel.meshCount = 0;
        s_cubeModelCreated = false;
    }
}

// Optimized: Batch texture changes
static Texture2D s_currentCubeTexture = {0};
static void DrawTexturedCube(Vector3 position, Vector3 size, Texture2D texture) {
    Model* cubeModel = GetCubeModel();
    
    // Only update texture if it changed (batching optimization)
    if (s_currentCubeTexture.id != texture.id) {
        cubeModel->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        s_currentCubeTexture = texture;
    }
    
    // Draw scaled and positioned
    DrawModelEx(*cubeModel, position, (Vector3){0, 1, 0}, 0.0f, size, WHITE);
}

// Static plane model for textured rendering
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

// Optimized: Batch texture changes
static Texture2D s_currentPlaneTexture = {0};
static void DrawTexturedPlane(Vector3 position, Vector2 size, Texture2D texture) {
    Model* planeModel = GetPlaneModel();
    
    // Only update texture if it changed (batching optimization)
    if (s_currentPlaneTexture.id != texture.id) {
        planeModel->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        s_currentPlaneTexture = texture;
    }
    
    // Draw scaled and positioned
    DrawModelEx(*planeModel, position, (Vector3){0, 1, 0}, 0.0f, (Vector3){size.x, 1.0f, size.y}, WHITE);
}

static void CleanupPlaneModel(void) {
    if (s_planeModelCreated && s_planeModel.meshCount > 0) {
        UnloadModel(s_planeModel);
        s_planeModel.meshCount = 0;
        s_planeModelCreated = false;
    }
}

// Render the maze in 3D with textures
static void RenderMaze(const Maze* maze, const GameAssets* assets) {
    if (!maze || !assets || !assets->loaded) return;
    
    const float halfCell = maze->cellSize * 0.5f;
    const float wallHalfHeight = WALL_HEIGHT * 0.5f;
    
    // Calculate maze bounds for floor/ceiling
    float mazeWidth = maze->width * maze->cellSize;
    float mazeHeight = maze->height * maze->cellSize;
    
    // Draw textured floor
    DrawTexturedPlane((Vector3){0, 0, 0}, (Vector2){mazeWidth, mazeHeight}, assets->floorTexture);
    
    // Draw textured ceiling
    DrawTexturedPlane((Vector3){0, WALL_HEIGHT, 0}, (Vector2){mazeWidth, mazeHeight}, assets->ceilingTexture);
    
    // Draw textured walls for each cell (texture is batched automatically by DrawTexturedCube)
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            
            // North wall
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                DrawTexturedCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ - halfCell
                }, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
            }
            
            // South wall
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                DrawTexturedCube((Vector3){
                    worldX,
                    wallHalfHeight,
                    worldZ + halfCell
                }, (Vector3){maze->cellSize, WALL_HEIGHT, WALL_THICK}, assets->wallTexture);
            }
            
            // West wall
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                DrawTexturedCube((Vector3){
                    worldX - halfCell,
                    wallHalfHeight,
                    worldZ
                }, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
            }
            
            // East wall
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                DrawTexturedCube((Vector3){
                    worldX + halfCell,
                    wallHalfHeight,
                    worldZ
                }, (Vector3){WALL_THICK, WALL_HEIGHT, maze->cellSize}, assets->wallTexture);
            }
        }
    }
    
    // Reset texture tracking for next frame
    s_currentCubeTexture.id = 0;
    s_currentPlaneTexture.id = 0;
    
    // Highlight exit cell (green floor)
    Vector2 exitWorld = Maze_CellToWorld(maze, (int)maze->exitPos.x, (int)maze->exitPos.y);
    DrawPlane((Vector3){exitWorld.x, 0.01f, exitWorld.y}, 
              (Vector2){maze->cellSize * 0.8f, maze->cellSize * 0.8f}, 
              (Color){0, 200, 0, 255});
}

int main(void) {
    // Initialize random seed
    srand((unsigned int)time(NULL));
    
    // Window setup
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "3D Maze Game | WASD+mouse, Shift run, Space jump, F toggle mouse, R restart");
    SetTargetFPS(120);
    
    bool mouseCaptured = true;
    DisableCursor();
    
    // Game state
    Maze* maze = NULL;
    WallRect* walls = NULL;
    int wallCount = 0;
    GameState gameState = GAME_STATE_PLAYING;
    
    // Player state
    Vector3 playerPos = {0.0f, 0.0f, 0.0f};
    float playerVelY = 0.0f;
    bool onGround = true;
    
    float yaw = 0.0f;      // Horizontal rotation (0: looking +Z)
    float pitch = 0.0f;    // Vertical rotation
    const float PITCH_LIMIT = DEG2RAD * 89.0f;
    
    // Camera
    Camera3D cam = {0};
    cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
    cam.target = (Vector3){0, 0, 1};
    cam.up = (Vector3){0, 1, 0};
    cam.fovy = 75.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    
    // Load assets
    GameAssets* assets = Assets_Load();
    if (!assets) {
        TraceLog(LOG_ERROR, "Failed to load assets!");
        CloseWindow();
        return 1;
    }
    
    // Torch and particle system state
    Torch* torches = NULL;
    int torchCount = 0;
    ParticleSystem** particleSystems = NULL;
    
    // Scary characters
    ScaryCharacter scaryChars[SCARY_CHAR_COUNT] = {0};
    
    // Initialize game
    InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
             &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT);
    
    // Main game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Toggle mouse capture
        if (IsKeyPressed(KEY_F)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) DisableCursor();
            else EnableCursor();
        }
        
        // Restart game
        if (IsKeyPressed(KEY_R)) {
            InitGame(&maze, &walls, &wallCount, &playerPos, &yaw, &pitch, &gameState,
                     &torches, &torchCount, &particleSystems, scaryChars, SCARY_CHAR_COUNT);
        }
        
        // Update torches
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
        
        // --- Mouse look (FPS) ---
        if (mouseCaptured && gameState == GAME_STATE_PLAYING) {
            Vector2 md = GetMouseDelta();
            yaw -= md.x * MOUSE_SENS;
            pitch -= md.y * MOUSE_SENS;
            if (pitch > PITCH_LIMIT) pitch = PITCH_LIMIT;
            if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
        }
        
        // Calculate forward and right vectors
        Vector3 forward = (Vector3){
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };
        Vector3 right = (Vector3){cosf(yaw), 0.0f, -sinf(yaw)};
        
        // --- Movement input (only when playing) ---
        if (gameState == GAME_STATE_PLAYING) {
            float speed = MOVE_SPEED * (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_MULTIPLIER : 1.0f);
            Vector2 wish = (Vector2){0.0f, 0.0f}; // XZ move wish
            
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
            
            // Normalize wish direction
            float len = sqrtf(wish.x * wish.x + wish.y * wish.y);
            if (len > 0.0001f) {
                wish.x /= len;
                wish.y /= len;
            }
            
            // Try to move in X then Z with collision (slide along walls)
            Vector2 pXZ = (Vector2){playerPos.x, playerPos.z};
            Vector2 step = (Vector2){wish.x * speed * dt, wish.y * speed * dt};
            
            // X axis movement
            Vector2 testX = (Vector2){pXZ.x + step.x, pXZ.y};
            if (!CollidesAny(testX, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.x = testX.x;
            }
            
            // Z axis movement
            Vector2 testZ = (Vector2){pXZ.x, pXZ.y + step.y};
            if (!CollidesAny(testZ, PLAYER_RADIUS, walls, wallCount)) {
                pXZ.y = testZ.y;
            }
            
            // Apply XZ back to 3D position
            playerPos.x = pXZ.x;
            playerPos.z = pXZ.y;
            
            // --- Jump + gravity ---
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
            
            // Ceiling clamp
            float maxFeetY = WALL_HEIGHT - PLAYER_EYE_HEIGHT;
            if (playerPos.y > maxFeetY) {
                playerPos.y = maxFeetY;
                if (playerVelY > 0) playerVelY = 0;
            }
            
            // Update scary characters to follow player
            if (gameState == GAME_STATE_PLAYING) {
                Vector2 playerPos2D = (Vector2){playerPos.x, playerPos.z};
                
                // Update each scary character
                for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                    Vector2 charPos = (Vector2){scaryChars[i].position.x, scaryChars[i].position.z};
                    
                    // Calculate direction to player
                    Vector2 dir = (Vector2){
                        playerPos2D.x - charPos.x,
                        playerPos2D.y - charPos.y
                    };
                    
                    // Normalize direction
                    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if (dist > 0.001f) {
                        dir.x /= dist;
                        dir.y /= dist;
                        
                        // Move towards player with collision detection
                        Vector2 moveStep = (Vector2){
                            dir.x * scaryChars[i].speed * dt,
                            dir.y * scaryChars[i].speed * dt
                        };
                        
                        // Try X movement
                        Vector2 testX = (Vector2){charPos.x + moveStep.x, charPos.y};
                        if (!CollidesAny(testX, scaryChars[i].radius, walls, wallCount)) {
                            charPos.x = testX.x;
                        }
                        
                        // Try Z movement
                        Vector2 testZ = (Vector2){charPos.x, charPos.y + moveStep.y};
                        if (!CollidesAny(testZ, scaryChars[i].radius, walls, wallCount)) {
                            charPos.y = testZ.y;
                        }
                        
                        scaryChars[i].position.x = charPos.x;
                        scaryChars[i].position.z = charPos.y;
                    }
                    
                    // Check collision with player
                    if (CircleCircleIntersect(playerPos2D, PLAYER_RADIUS, charPos, scaryChars[i].radius)) {
                        gameState = GAME_STATE_GAMEOVER;
                        break; // Exit loop once collision detected
                    }
                }
            }
            
            // Check if player reached exit
            int cellX, cellY;
            Maze_WorldToCell(maze, playerPos.x, playerPos.z, &cellX, &cellY);
            if (Maze_IsExit(maze, cellX, cellY)) {
                gameState = GAME_STATE_WON;
            }
        }
        
        // Update camera from player
        cam.position = (Vector3){playerPos.x, playerPos.y + PLAYER_EYE_HEIGHT, playerPos.z};
        cam.target = (Vector3){
            cam.position.x + forward.x,
            cam.position.y + forward.y,
            cam.position.z + forward.z
        };
        
        // ----------- RENDER -----------
        BeginDrawing();
        ClearBackground((Color){5, 5, 8, 255}); // Much darker background for scary atmosphere
        
        BeginMode3D(cam);
        
        // Render maze with textures
        if (maze) {
            RenderMaze(maze, assets);
        }
        
        // Render torches
        if (torches && torchCount > 0) {
            Torches_Render(torches, torchCount);
            
            // Render flickering light sources (more dramatic flicker for scary atmosphere)
            for (int i = 0; i < torchCount; i++) {
                // More dramatic flickering with random spikes
                float flicker = 0.5f + 0.4f * sinf(torches[i].flickerTime) + 
                                0.15f * sinf(torches[i].flickerTime * 3.5f) +
                                0.1f * sinf(torches[i].flickerTime * 7.0f);
                // Add occasional dramatic drops
                if ((int)(torches[i].flickerTime * 10) % 23 == 0) {
                    flicker *= 0.3f; // Sudden dimming
                }
                float intensity = torches[i].baseIntensity * flicker;
                
                Vector3 lightPos = torches[i].position;
                lightPos.y += 0.3f;
                
                // Draw light glow (using cube for better performance, dimmer for atmosphere)
                Color lightColor = (Color){
                    (unsigned char)(220 * intensity), // Slightly dimmer
                    (unsigned char)(150 * intensity),
                    (unsigned char)(80 * intensity),
                    255
                };
                float lightSize = 0.12f * intensity; // Slightly smaller
                DrawCube(lightPos, lightSize, lightSize, lightSize, lightColor);
            }
            
            // Render particle systems (flames)
            if (particleSystems) {
                for (int i = 0; i < torchCount; i++) {
                    if (particleSystems[i]) {
                        ParticleSystem_Render(particleSystems[i]);
                    }
                }
            }
        }
        
        // Render scary characters (dark, menacing figures)
        if (gameState == GAME_STATE_PLAYING || gameState == GAME_STATE_GAMEOVER) {
            for (int i = 0; i < SCARY_CHAR_COUNT; i++) {
                Vector3 charRenderPos = scaryChars[i].position;
                charRenderPos.y = scaryChars[i].height * 0.5f; // Center the character vertically
                
                // Draw a dark, scary character (dark red/black cube with slight glow)
                // Slightly vary the color for each character
                Color scaryColor = (Color){
                    (unsigned char)(40 + i * 5), 
                    0, 
                    (unsigned char)(i * 3), 
                    255
                }; // Very dark red with slight variation
                DrawCube(charRenderPos, scaryChars[i].radius * 2.0f, scaryChars[i].height, scaryChars[i].radius * 2.0f, scaryColor);
                
                // Add a subtle dark glow around it
                DrawCubeWires(charRenderPos, scaryChars[i].radius * 2.2f, scaryChars[i].height * 1.1f, scaryChars[i].radius * 2.2f, (Color){80, 0, 0, 100});
            }
        }
        
        EndMode3D();
        
        // Crosshair
        if (gameState == GAME_STATE_PLAYING) {
            int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
            DrawLine(cx - 8, cy, cx + 8, cy, RAYWHITE);
            DrawLine(cx, cy - 8, cx, cy + 8, RAYWHITE);
        }
        
        // HUD
        if (gameState == GAME_STATE_PLAYING) {
            DrawText("WASD: move | Shift: run | Space: jump | F: toggle mouse | R: restart | Esc: quit",
                     20, 20, 18, RAYWHITE);
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
            DrawText(winText, (screenWidth - textWidth) / 2, screenHeight / 2 - 60, fontSize, GREEN);
            
            // Restart instruction
            const char* restartText = "Press R to restart or Esc to quit";
            fontSize = 24;
            textWidth = MeasureText(restartText, fontSize);
            DrawText(restartText, (screenWidth - textWidth) / 2, screenHeight / 2 + 20, fontSize, RAYWHITE);
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
