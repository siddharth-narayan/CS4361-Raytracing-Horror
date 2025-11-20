#include "../include/maze.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper: Get cell index from coordinates
static int CellIndex(const Maze* maze, int x, int y) {
    if (x < 0 || x >= maze->width || y < 0 || y >= maze->height) return -1;
    return y * maze->width + x;
}

// Helper: Check if cell is valid and unvisited
static bool IsValidUnvisited(const Maze* maze, int x, int y, bool* visited) {
    int idx = CellIndex(maze, x, y);
    if (idx < 0) return false;
    return !visited[idx];
}

// Helper: Get random neighbor for DFS
static int GetRandomNeighbor(const Maze* maze, int x, int y, bool* visited, int* outDir) {
    int dirs[4] = {MAZE_NORTH, MAZE_EAST, MAZE_SOUTH, MAZE_WEST};
    int dx[4] = {0, 1, 0, -1};
    int dy[4] = {-1, 0, 1, 0};
    
    // Shuffle directions for randomness
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = temp;
        temp = dx[i];
        dx[i] = dx[j];
        dx[j] = temp;
        temp = dy[i];
        dy[i] = dy[j];
        dy[j] = temp;
    }
    
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (IsValidUnvisited(maze, nx, ny, visited)) {
            *outDir = dirs[i];
            return 1;
        }
    }
    return 0;
}

// Create a new maze structure
Maze* Maze_Create(int width, int height, float cellSize) {
    if (width < 1 || height < 1 || cellSize <= 0.0f) return NULL;
    
    Maze* maze = (Maze*)malloc(sizeof(Maze));
    if (!maze) return NULL;
    
    maze->width = width;
    maze->height = height;
    maze->cellSize = cellSize;
    maze->cells = (unsigned char*)calloc(width * height, sizeof(unsigned char));
    
    if (!maze->cells) {
        free(maze);
        return NULL;
    }
    
    // Initialize all cells with all walls
    for (int i = 0; i < width * height; i++) {
        maze->cells[i] = MAZE_ALL;
    }
    
    maze->startPos = (Vector2){0, 0};
    maze->exitPos = (Vector2){width - 1, height - 1};
    
    return maze;
}

// Destroy maze and free memory
void Maze_Destroy(Maze* maze) {
    if (maze) {
        free(maze->cells);
        free(maze);
    }
}

// Generate maze using DFS backtracking algorithm
void Maze_Generate(Maze* maze) {
    if (!maze || !maze->cells) return;
    
    // Initialize visited array
    bool* visited = (bool*)calloc(maze->width * maze->height, sizeof(bool));
    if (!visited) return;
    
    // Stack for backtracking
    typedef struct { int x, y; } StackCell;
    StackCell* stack = (StackCell*)malloc(maze->width * maze->height * sizeof(StackCell));
    if (!stack) {
        free(visited);
        return;
    }
    
    int stackTop = 0;
    
    // Start from (0, 0)
    int startX = 0, startY = 0;
    visited[CellIndex(maze, startX, startY)] = true;
    stack[stackTop++] = (StackCell){startX, startY};
    
    // Direction mappings
    int oppositeDir[4] = {MAZE_SOUTH, MAZE_WEST, MAZE_NORTH, MAZE_EAST};
    int dirs[4] = {MAZE_NORTH, MAZE_EAST, MAZE_SOUTH, MAZE_WEST};
    int dx[4] = {0, 1, 0, -1};
    int dy[4] = {-1, 0, 1, 0};
    
    // DFS backtracking
    while (stackTop > 0) {
        int x = stack[stackTop - 1].x;
        int y = stack[stackTop - 1].y;
        
        int dir;
        if (GetRandomNeighbor(maze, x, y, visited, &dir)) {
            // Find direction index
            int dirIdx = 0;
            for (int i = 0; i < 4; i++) {
                if (dirs[i] == dir) {
                    dirIdx = i;
                    break;
                }
            }
            
            int nx = x + dx[dirIdx];
            int ny = y + dy[dirIdx];
            
            // Remove walls between current and neighbor
            int currIdx = CellIndex(maze, x, y);
            int nextIdx = CellIndex(maze, nx, ny);
            
            maze->cells[currIdx] &= ~dir;
            maze->cells[nextIdx] &= ~oppositeDir[dirIdx];
            
            visited[nextIdx] = true;
            stack[stackTop++] = (StackCell){nx, ny};
        } else {
            // Backtrack
            stackTop--;
        }
    }
    
    free(stack);
    free(visited);
}

// Check if a cell has a wall in the given direction
bool Maze_HasWall(const Maze* maze, int x, int y, int direction) {
    int idx = CellIndex(maze, x, y);
    if (idx < 0) return true; // Out of bounds = wall
    return (maze->cells[idx] & direction) != 0;
}

// Get all wall rectangles for collision detection
int Maze_GetWallRects(const Maze* maze, WallRect* outRects, int maxRects) {
    if (!maze || !outRects || maxRects <= 0) return 0;
    
    int count = 0;
    const float wallThick = 0.1f; // Wall thickness for collision
    
    // Check each cell
    for (int y = 0; y < maze->height && count < maxRects; y++) {
        for (int x = 0; x < maze->width && count < maxRects; x++) {
            // Calculate cell center (matching RenderMaze coordinates)
            float cellCenterX = (x - maze->width * 0.5f + 0.5f) * maze->cellSize;
            float cellCenterZ = (y - maze->height * 0.5f + 0.5f) * maze->cellSize;
            float halfCell = maze->cellSize * 0.5f;
            
            // North wall (at north edge of cell)
            if (Maze_HasWall(maze, x, y, MAZE_NORTH)) {
                outRects[count].rect = (Rectangle){
                    cellCenterX - halfCell - wallThick * 0.5f,
                    cellCenterZ - halfCell - wallThick * 0.5f,
                    maze->cellSize + wallThick,
                    wallThick
                };
                outRects[count].isVertical = false;
                count++;
            }
            
            // West wall (at west edge of cell)
            if (Maze_HasWall(maze, x, y, MAZE_WEST)) {
                outRects[count].rect = (Rectangle){
                    cellCenterX - halfCell - wallThick * 0.5f,
                    cellCenterZ - halfCell - wallThick * 0.5f,
                    wallThick,
                    maze->cellSize + wallThick
                };
                outRects[count].isVertical = true;
                count++;
            }
            
            // East wall (only for rightmost cells, at east edge)
            if (x == maze->width - 1 && Maze_HasWall(maze, x, y, MAZE_EAST)) {
                outRects[count].rect = (Rectangle){
                    cellCenterX + halfCell - wallThick * 0.5f,
                    cellCenterZ - halfCell - wallThick * 0.5f,
                    wallThick,
                    maze->cellSize + wallThick
                };
                outRects[count].isVertical = true;
                count++;
            }
            
            // South wall (only for bottommost cells, at south edge)
            if (y == maze->height - 1 && Maze_HasWall(maze, x, y, MAZE_SOUTH)) {
                outRects[count].rect = (Rectangle){
                    cellCenterX - halfCell - wallThick * 0.5f,
                    cellCenterZ + halfCell - wallThick * 0.5f,
                    maze->cellSize + wallThick,
                    wallThick
                };
                outRects[count].isVertical = false;
                count++;
            }
        }
    }
    
    return count;
}

// Convert cell coordinates to world position (center of cell)
Vector2 Maze_CellToWorld(const Maze* maze, int cellX, int cellY) {
    return (Vector2){
        (cellX - maze->width * 0.5f + 0.5f) * maze->cellSize,
        (cellY - maze->height * 0.5f + 0.5f) * maze->cellSize
    };
}

// Convert world position to cell coordinates
void Maze_WorldToCell(const Maze* maze, float worldX, float worldZ, int* outCellX, int* outCellY) {
    int cellX = (int)((worldX / maze->cellSize) + maze->width * 0.5f);
    int cellY = (int)((worldZ / maze->cellSize) + maze->height * 0.5f);
    
    if (outCellX) *outCellX = cellX;
    if (outCellY) *outCellY = cellY;
}

// Check if given cell coordinates are the exit
bool Maze_IsExit(const Maze* maze, int cellX, int cellY) {
    return (cellX == (int)maze->exitPos.x && cellY == (int)maze->exitPos.y);
}

