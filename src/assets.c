#include "../include/assets.h"
#include "../include/maze.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

Texture2D GenerateStoneWallTexture(int width, int height) {
    // Darker base color for horror atmosphere (5% lighter)
    Image img = GenImageColor(width, height, (Color){47, 47, 53, 255});
    
    // access the pixel data directly
    Color* pixels = (Color*)img.data;
    
    // add the stone-like noise and variation
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gridX = x % 32;
            int gridY = y % 32;
            bool isMortar = (gridX < 2 || gridY < 2 || gridX > 30 || gridY > 30);
            
            if (isMortar) {
                pixels[y * width + x] = (Color){26, 26, 32, 255};
            } else {
                // Add noise for stone texture (darker for horror)
                float noise = ((float)(rand() % 100) / 100.0f) * 0.3f;
                int baseR = 47 + (int)(noise * 26);
                int baseG = 47 + (int)(noise * 21);
                int baseB = 53 + (int)(noise * 16);
                pixels[y * width + x] = (Color){baseR, baseG, baseB, 255};
            }
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

// Generate procedural wooden floor texture
Texture2D GenerateWoodFloorTexture(int width, int height) {
    // Darker base color for horror atmosphere (5% lighter)
    Image img = GenImageColor(width, height, (Color){53, 37, 26, 255});
    
    // Access pixel data directly
    Color* pixels = (Color*)img.data;
    
    // Create wood grain pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Wood planks (horizontal strips)
            int plankHeight = 64;
            int plankIdx = y / plankHeight;
            
            // Add grain lines
            float grain = sinf((float)x * 0.1f + (float)plankIdx * 0.5f) * 0.1f;
            float variation = ((float)(rand() % 100) / 100.0f) * 0.2f;
            
            int r = 53 + (int)((grain + variation) * 26);
            int g = 37 + (int)((grain + variation) * 21);
            int b = 26 + (int)((grain + variation) * 16);
            
            // Plank boundaries (darker)
            if ((y % plankHeight) < 2) {
                r = (int)(r * 0.6f);
                g = (int)(g * 0.6f);
                b = (int)(b * 0.6f);
            }
            
            pixels[y * width + x] = (Color){r, g, b, 255};
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

// Generate simple ceiling texture
Texture2D GenerateCeilingTexture(int width, int height) {
    // Much darker ceiling for horror atmosphere (5% lighter)
    Image img = GenImageColor(width, height, (Color){21, 21, 26, 255});
    
    // Access pixel data directly
    Color* pixels = (Color*)img.data;
    
    // Add subtle noise (darker)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = ((float)(rand() % 100) / 100.0f) * 0.15f;
            int r = 21 + (int)(noise * 11);
            int g = 21 + (int)(noise * 11);
            int b = 26 + (int)(noise * 11);
            pixels[y * width + x] = (Color){r, g, b, 255};
        }
    }
    
    Texture2D texture = LoadTextureFromImage(img);
    UnloadImage(img);  // raylib will free the memory it allocated
    return texture;
}

GameAssets* Assets_Load(void) {
    GameAssets* assets = (GameAssets*)malloc(sizeof(GameAssets));
    if (!assets) return NULL;
    
    assets->wallTexture = GenerateStoneWallTexture(256, 256);
    assets->floorTexture = GenerateWoodFloorTexture(256, 256);
    assets->ceilingTexture = GenerateCeilingTexture(256, 256);
    
    assets->horrorMusic = LoadMusicStream("assets/horror.mp3");
    if (assets->horrorMusic.frameCount == 0) {
        assets->horrorMusic = LoadMusicStream("horror.mp3");
    }
    
    if (assets->horrorMusic.frameCount > 0) {
        assets->horrorMusic.looping = true;
        TraceLog(LOG_INFO, "Horror music loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load horror.mp3 - continuing without music");
    }
    
    // Load jumpscare sound - try assets directory first, then current directory
    assets->jumpScareSound = LoadSound("assets/jump.mp3");
    if (assets->jumpScareSound.frameCount == 0) {
        assets->jumpScareSound = LoadSound("jump.mp3");
    }
    
    if (assets->jumpScareSound.frameCount > 0) {
        TraceLog(LOG_INFO, "Jumpscare sound loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load jump.mp3 - continuing without jumpscare sound");
    }
    
    // Load footstep sound
    assets->footstepSound = LoadSound("assets/footstep.mp3");
    if (assets->footstepSound.frameCount == 0) {
        assets->footstepSound = LoadSound("footstep.mp3");
    }
    
    if (assets->footstepSound.frameCount > 0) {
        TraceLog(LOG_INFO, "Footstep sound loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load footstep.mp3 - continuing without footstep sounds");
    }
    
    // Load battery pickup sound (try assets directory first, then current directory)
    assets->batteryPickupSound = LoadSound("assets/battery.mp3");
    if (assets->batteryPickupSound.frameCount == 0) {
        assets->batteryPickupSound = LoadSound("battery.mp3");
    }
    
    if (assets->batteryPickupSound.frameCount > 0) {
        TraceLog(LOG_INFO, "Battery pickup sound loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load battery.mp3 - continuing without battery pickup sound");
    }
    
    // Load bullet sound (try assets directory first, then current directory)
    assets->bulletSound = LoadSound("assets/bullet.mp3");
    if (assets->bulletSound.frameCount == 0) {
        assets->bulletSound = LoadSound("bullet.mp3");
    }
    
    if (assets->bulletSound.frameCount > 0) {
        TraceLog(LOG_INFO, "Bullet sound loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load bullet.mp3 - continuing without bullet sound");
    }
    
    // Load victory sound (try assets directory first, then current directory)
    assets->victorySound = LoadSound("assets/victory.mp3");
    if (assets->victorySound.frameCount == 0) {
        assets->victorySound = LoadSound("victory.mp3");
    }
    
    if (assets->victorySound.frameCount > 0) {
        TraceLog(LOG_INFO, "Victory sound loaded successfully");
    } else {
        TraceLog(LOG_WARNING, "Failed to load victory.mp3 - continuing without victory sound");
    }
    
    assets->loaded = true;
    
    return assets;
}

void Assets_Unload(GameAssets* assets) {
    if (!assets || !assets->loaded) return;
    
    UnloadTexture(assets->wallTexture);
    UnloadTexture(assets->floorTexture);
    UnloadTexture(assets->ceilingTexture);
    
    if (assets->horrorMusic.frameCount > 0) {
        UnloadMusicStream(assets->horrorMusic);
    }
    
    if (assets->jumpScareSound.frameCount > 0) {
        UnloadSound(assets->jumpScareSound);
    }
    
    if (assets->footstepSound.frameCount > 0) {
        UnloadSound(assets->footstepSound);
    }
    
    if (assets->batteryPickupSound.frameCount > 0) {
        UnloadSound(assets->batteryPickupSound);
    }
    
    if (assets->bulletSound.frameCount > 0) {
        UnloadSound(assets->bulletSound);
    }
    
    if (assets->victorySound.frameCount > 0) {
        UnloadSound(assets->victorySound);
    }
    
    assets->loaded = false;
    free(assets);
}

int Torches_Generate(const Maze* maze, Torch** outTorches, int maxTorches) {
    if (!maze || !outTorches || maxTorches <= 0) return 0;
    
    *outTorches = (Torch*)malloc(maxTorches * sizeof(Torch));
    if (!*outTorches) return 0;
    
    int count = 0;
    const float torchHeight = 2.0f;
    const float wallOffset = 0.11f;
    const float torchPlacementChance = 0.08f;
    
    typedef struct {
        int x, y;
        int direction; // 0=N, 1=S, 2=W, 3=E
        float worldX, worldZ;
    } WallPos;
    
    WallPos* walls = (WallPos*)malloc(maze->width * maze->height * 4 * sizeof(WallPos));
    int wallCount = 0;
    
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            float worldX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float worldZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            float halfCell = maze->cellSize * 0.5f;
            
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                walls[wallCount++] = (WallPos){x, y, 0, worldX, worldZ - halfCell};
            }
            if (Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                walls[wallCount++] = (WallPos){x, y, 1, worldX, worldZ + halfCell};
            }
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                walls[wallCount++] = (WallPos){x, y, 2, worldX - halfCell, worldZ};
            }
            if (Maze_HasWall(maze, x, y, MAZE_EAST)) {
                walls[wallCount++] = (WallPos){x, y, 3, worldX + halfCell, worldZ};
            }
        }
    }
    
    // Randomly place torches on a small percentage of walls
    for (int i = 0; i < wallCount && count < maxTorches; i++) {
        // Random chance to place a torch on this wall
        if ((float)rand() / (float)RAND_MAX < torchPlacementChance) {
            WallPos* wall = &walls[i];
            float halfCell = maze->cellSize * 0.5f;
            
            // Random position along the wall
            float randomOffset = ((float)rand() / (float)RAND_MAX) * (maze->cellSize - 0.5f) + 0.25f;
            
            switch (wall->direction) {
                case 0: // North
                    (*outTorches)[count].position = (Vector3){
                        wall->worldX - halfCell + randomOffset,
                        torchHeight,
                        wall->worldZ - wallOffset
                    };
                    (*outTorches)[count].normal = (Vector3){0, 0, 1};
                    break;
                case 1: // South
                    (*outTorches)[count].position = (Vector3){
                        wall->worldX - halfCell + randomOffset,
                        torchHeight,
                        wall->worldZ + wallOffset
                    };
                    (*outTorches)[count].normal = (Vector3){0, 0, -1};
                    break;
                case 2: // West
                    (*outTorches)[count].position = (Vector3){
                        wall->worldX - wallOffset,
                        torchHeight,
                        wall->worldZ - halfCell + randomOffset
                    };
                    (*outTorches)[count].normal = (Vector3){1, 0, 0};
                    break;
                case 3: // East
                    (*outTorches)[count].position = (Vector3){
                        wall->worldX + wallOffset,
                        torchHeight,
                        wall->worldZ - halfCell + randomOffset
                    };
                    (*outTorches)[count].normal = (Vector3){-1, 0, 0};
                    break;
            }
            
            (*outTorches)[count].flickerTime = (float)(rand() % 1000) / 1000.0f * 6.28f;
            (*outTorches)[count].baseIntensity = 0.6f + ((float)(rand() % 30) / 100.0f);
            
            count++;
        }
    }
    
    free(walls);
    return count;
}

// update the torch flickering
void Torches_Update(Torch* torches, int count, float dt) {
    for (int i = 0; i < count; i++) {
        float speed = 1.5f + 0.3f * sinf(torches[i].flickerTime * 0.3f);
        torches[i].flickerTime += dt * speed;
        if (torches[i].flickerTime > 6.28f) {
            torches[i].flickerTime -= 6.28f;
        }
    }
}

// render the torches (simple cube representation)
void Torches_Render(const Torch* torches, int count) {
    // Draw simple cubes for torches
    for (int i = 0; i < count; i++) {
        DrawCube(torches[i].position, 0.1f, 0.3f, 0.1f, (Color){60, 40, 20, 255});
        // draw the torch bracket
        Vector3 bracketPos = torches[i].position;
        bracketPos.y += 0.15f;
        DrawCube(bracketPos, 0.15f, 0.05f, 0.05f, (Color){80, 80, 80, 255});
    }
}

// Create particle system
ParticleSystem* ParticleSystem_Create(int maxParticles) {
    ParticleSystem* ps = (ParticleSystem*)malloc(sizeof(ParticleSystem));
    if (!ps) return NULL;
    
    ps->particles = (Particle*)malloc(maxParticles * sizeof(Particle));
    if (!ps->particles) {
        free(ps);
        return NULL;
    }
    
    ps->maxParticles = maxParticles;
    ps->activeParticles = 0;
    ps->emitRate = 8.0f;  // Reduced from 15.0f for smoother effect
    ps->emitAccumulator = 0.0f;
    ps->emitterPos = (Vector3){0, 0, 0};
    
    return ps;
}

// Destroy particle system
void ParticleSystem_Destroy(ParticleSystem* ps) {
    if (!ps) return;
    if (ps->particles) free(ps->particles);
    free(ps);
}

// Update particle system
void ParticleSystem_Update(ParticleSystem* ps, Vector3 emitterPos, float dt) {
    if (!ps) return;
    
    ps->emitterPos = emitterPos;
    
    // Emit new particles
    ps->emitAccumulator += ps->emitRate * dt;
    int toEmit = (int)ps->emitAccumulator;
    ps->emitAccumulator -= (float)toEmit;
    
    for (int i = 0; i < toEmit && ps->activeParticles < ps->maxParticles; i++) {
        Particle* p = &ps->particles[ps->activeParticles];
        p->position = emitterPos;
        p->position.y += 0.25f; // Slight offset above torch
        
        // Much smoother, more vertical movement with minimal horizontal spread
        float horizontalSpread = 0.02f;  // Reduced from ~0.2f
        float verticalSpeed = 0.15f + ((float)(rand() % 30) / 1000.0f);  // Reduced from ~0.6f
        
        p->velocity = (Vector3){
            ((float)(rand() % 100) - 50.0f) / 2500.0f * horizontalSpread,  // Much smaller X movement
            verticalSpeed,  // Smoother upward movement
            ((float)(rand() % 100) - 50.0f) / 2500.0f * horizontalSpread   // Much smaller Z movement
        };
        p->life = 1.0f;
        p->maxLife = 0.8f + ((float)(rand() % 40) / 100.0f);  // Longer lifetime for smoother fade
        p->size = 0.06f + ((float)(rand() % 20) / 1000.0f);  // Less size variation
        p->color = (Color){
            255,
            150 + (rand() % 50),
            0,
            255
        };
        ps->activeParticles++;
    }
    
    // Update existing particles
    for (int i = 0; i < ps->activeParticles; i++) {
        Particle* p = &ps->particles[i];
        
        // Update physics with smoother, gentler movement
        p->velocity.y += -1.2f * dt; // Reduced gravity for smoother, slower fall
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->position.z += p->velocity.z * dt;
        
        // Update life
        p->life -= dt;
        
        // Remove dead particles (swap with last)
        if (p->life <= 0.0f) {
            ps->particles[i] = ps->particles[ps->activeParticles - 1];
            ps->activeParticles--;
            i--;
        }
    }
}

// Render particle system (optimized - using simple cubes instead of spheres)
void ParticleSystem_Render(const ParticleSystem* ps) {
    if (!ps) return;
    
    // Use simple cubes instead of spheres for better performance
    for (int i = 0; i < ps->activeParticles; i++) {
        const Particle* p = &ps->particles[i];
        
        // Calculate alpha based on life
        float alpha = p->life / p->maxLife;
        Color renderColor = p->color;
        renderColor.a = (unsigned char)(alpha * 255.0f);
        
        // Draw as small cube (much faster than sphere)
        float size = p->size * 2.0f; // Scale up slightly for visibility
        DrawCube(p->position, size, size, size, renderColor);
    }
}

void Lighting_UpdateTorchLights(const Torch* torches, int count, float time) {
    (void)torches;
    (void)count;
    (void)time;
}

// Generate battery pickups in the maze
int Batteries_Generate(const Maze* maze, BatteryPickup** outBatteries, int maxBatteries) {
    if (!maze || !outBatteries || maxBatteries <= 0) return 0;
    
    *outBatteries = (BatteryPickup*)malloc(maxBatteries * sizeof(BatteryPickup));
    if (!*outBatteries) return 0;
    
    int count = 0;
    const float batteryHeight = 0.8f; // Height above ground
    const float batteryPlacementChance = 0.12f; // 12% chance per cell
    
    // Place batteries randomly in open cells (not on walls, not at start/exit)
    for (int y = 0; y < maze->height && count < maxBatteries; y++) {
        for (int x = 0; x < maze->width && count < maxBatteries; x++) {
            // Skip start and exit positions
            bool isStart = (x == (int)maze->startPos.x && y == (int)maze->startPos.y);
            bool isExit = (x == (int)maze->exitPos.x && y == (int)maze->exitPos.y);
            
            if (isStart || isExit) continue;
            
            // Random chance to place a battery in this cell
            if ((float)rand() / (float)RAND_MAX < batteryPlacementChance) {
                Vector2 worldPos = Maze_CellToWorld(maze, x, y);
                
                // Add some random offset within the cell for variety
                float offsetX = ((float)(rand() % 100) / 100.0f - 0.5f) * maze->cellSize * 0.6f;
                float offsetZ = ((float)(rand() % 100) / 100.0f - 0.5f) * maze->cellSize * 0.6f;
                
                (*outBatteries)[count].position = (Vector3){
                    worldPos.x + offsetX,
                    batteryHeight,
                    worldPos.y + offsetZ
                };
                (*outBatteries)[count].collected = false;
                (*outBatteries)[count].bobTime = (float)(rand() % 1000) / 1000.0f * 6.28f; // Random starting phase
                (*outBatteries)[count].rotation = (float)(rand() % 360) * DEG2RAD;
                
                count++;
            }
        }
    }
    
    return count;
}

// Update battery animations
void Batteries_Update(BatteryPickup* batteries, int count, float dt) {
    if (!batteries) return;
    
    for (int i = 0; i < count; i++) {
        if (!batteries[i].collected) {
            // Bob up and down
            batteries[i].bobTime += dt * 2.0f; // Animation speed
            if (batteries[i].bobTime > 6.28f) {
                batteries[i].bobTime -= 6.28f;
            }
            
            // Rotate slowly
            batteries[i].rotation += dt * 1.5f;
            if (batteries[i].rotation > 6.28f) {
                batteries[i].rotation -= 6.28f;
            }
        }
    }
}

// Render battery pickups with visual effects
void Batteries_Render(const BatteryPickup* batteries, int count, float globalTime) {
    if (!batteries) return;
    
    for (int i = 0; i < count; i++) {
        if (batteries[i].collected) continue;
        
        Vector3 pos = batteries[i].position;
        
        // Bob animation (vertical movement)
        float bobOffset = sinf(batteries[i].bobTime) * 0.15f;
        pos.y = batteries[i].position.y + bobOffset;
        
        // Battery size
        float batterySize = 0.25f;
        
        // Main battery body (green/yellow glowing cube)
        Color batteryColor = (Color){100, 255, 100, 255}; // Bright green
        DrawCube(pos, batterySize * 0.6f, batterySize * 1.2f, batterySize * 0.4f, batteryColor);
        
        // Battery terminals (top and bottom)
        Color terminalColor = (Color){150, 255, 150, 255};
        Vector3 topTerminal = pos;
        topTerminal.y += batterySize * 0.6f;
        DrawCube(topTerminal, batterySize * 0.5f, batterySize * 0.2f, batterySize * 0.3f, terminalColor);
        
        Vector3 bottomTerminal = pos;
        bottomTerminal.y -= batterySize * 0.6f;
        DrawCube(bottomTerminal, batterySize * 0.5f, batterySize * 0.2f, batterySize * 0.3f, terminalColor);
        
        // Glowing effect (pulsing sphere around battery)
        float pulse = 0.7f + 0.3f * sinf(globalTime * 3.0f + i);
        float glowSize = batterySize * 1.5f * pulse;
        Color glowColor = (Color){
            (unsigned char)(100 * pulse),
            (unsigned char)(255 * pulse),
            (unsigned char)(100 * pulse),
            (unsigned char)(80 * pulse)
        };
        DrawSphere(pos, glowSize, glowColor);
        
        // Light pool on the floor (similar to torches but smaller)
        Vector3 floorLightPos = (Vector3){pos.x, 0.05f, pos.z};
        float poolRadius = 0.6f * pulse;
        float poolAlpha = 0.3f * pulse;
        Color poolColor = (Color){
            (unsigned char)(100 * pulse),
            (unsigned char)(255 * pulse),
            (unsigned char)(100 * pulse),
            (unsigned char)(poolAlpha * 255)
        };
        
        // Draw light pool as concentric circles
        for (int layer = 0; layer < 3; layer++) {
            float layerRadius = poolRadius * (1.0f - layer * 0.33f);
            float layerAlpha = poolAlpha * (1.0f - layer * 0.4f);
            if (layerRadius > 0.1f) {
                Color layerColor = poolColor;
                layerColor.a = (unsigned char)(layerAlpha * 255);
                DrawPlane(floorLightPos, (Vector2){layerRadius * 2.0f, layerRadius * 2.0f}, layerColor);
            }
        }
    }
}

