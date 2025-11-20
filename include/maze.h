#pragma once

#include "raylib.h"
#include <stdbool.h>

// Maze cell directions (bit flags for walls)
#define MAZE_NORTH 0x01
#define MAZE_EAST  0x02
#define MAZE_SOUTH 0x04
#define MAZE_WEST  0x08
#define MAZE_ALL   0x0F

// Maze structure
typedef struct {
    int width;          // Number of cells horizontally
    int height;         // Number of cells vertically
    unsigned char* cells; // Cell data: each byte stores wall flags
    float cellSize;     // Size of each cell in world units
    Vector2 startPos;   // Starting position (cell coordinates)
    Vector2 exitPos;    // Exit position (cell coordinates)
} Maze;

// Wall rectangle for collision detection
typedef struct {
    Rectangle rect;     // Collision rectangle in XZ plane
    bool isVertical;    // true for N/S walls, false for E/W walls
} WallRect;

// Function declarations
Maze* Maze_Create(int width, int height, float cellSize);
void Maze_Destroy(Maze* maze);
void Maze_Generate(Maze* maze);
bool Maze_HasWall(const Maze* maze, int x, int y, int direction);
int Maze_GetWallRects(const Maze* maze, WallRect* outRects, int maxRects);
Vector2 Maze_CellToWorld(const Maze* maze, int cellX, int cellY);
void Maze_WorldToCell(const Maze* maze, float worldX, float worldZ, int* outCellX, int* outCellY);
bool Maze_IsExit(const Maze* maze, int cellX, int cellY);

