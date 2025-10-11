#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <cblas.h>

int main() {
    double a[3][3] = { 1, 2, 3, 4, 5, 6, 7 ,8, 9};
    double b[3][3] = { 9, 8, 7, 6, 5, 4, 3, 2, 1};

    cblas_daxpy(9, -1, *a, 1,* b, 1);
    for (int i = 0; i < 3; i++) {
        printf("%.2f, %.2f, %.2f\n", b[i][0], b[i][1], b[i][2]);
    }

    return 0;
}

// void updateCircle(Vector2 *pos, Vector2 *velocity) {
//     if (pos->x > 500 && velocity->x > 0 || pos->x < 0 && velocity->x < 0) {
//         velocity->x -= velocity->x * 2;
//     } else if (pos->y > 500 && velocity->y > 0 || pos->y < 0 && velocity->y < 0) {
//         velocity->y -= velocity->y * 2;
//     }

//     printf("wegwegweg %f %f %f %f", pos->x, pos->y, velocity->x, velocity->y);
//     pos->x += GetFrameTime() * 10  * velocity->x;
//     pos->y += GetFrameTime() * 10 * velocity->y;
// }

// int main(void)
// {
//     InitWindow(800, 450, "raylib [core] example - basic window");

//     Vector2 circlePos = (Vector2){ 200, 200};
//     Vector2 circleVel = (Vector2){ 10, 10};

//     float rot = 0;
//     while (!WindowShouldClose())
//     {
//         Camera camera = { 0 };
//         rot += .05;
//         camera.position = (Vector3){ 10 * cosf(rot), 10.0f, 10 * sinf(rot)}; // Camera position
//         camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
//         camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
//         camera.fovy = 45.0f;                                // Camera field-of-view Y
//         camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type
//         Vector3 cubePosition = { 0.0f, 1.0f, 0.0f };
//         Vector3 cubeSize = { 2.0f, 2.0f, 2.0f };

//         SetTargetFPS(60);                   // Set our game to run at 60 frames-per-second
//         BeginDrawing();
//             ClearBackground(BEIGE);

//             BeginMode3D(camera);
//              DrawPlane((Vector3){0, 0, 0}, (Vector2){10, 10}, RED);   
//                 DrawCube(cubePosition, cubeSize.x, cubeSize.y, cubeSize.z, GRAY);
//                 DrawCubeWires(cubePosition, cubeSize.x, cubeSize.y, cubeSize.z, BLACK);
               
//             EndMode3D();

//             DrawText("Congrats! You created your first window!", 190, 225, 20, BLACK);

//             updateCircle(&circlePos, &circleVel);
//             DrawCircle(circlePos.x, circlePos.y, 64.5, BLUE);             
            
//         EndDrawing();
//     }

//     CloseWindow();

//     return 0;
// }