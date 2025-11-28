#include <raylib.h>

int main() {
    int w = 800, h = 600;
    InitWindow(w, h, "Raytrace");
    Shader shader = LoadShader(0, "./src/raytracer.fs");

    int resLoc = GetShaderLocation(shader, "uResolution");
    int timeLoc = GetShaderLocation(shader, "uTime");
    Vector2 res = { (float)w, (float)h };

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float t = GetTime();

        SetShaderValue(shader, resLoc, &res, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, timeLoc, &t, SHADER_UNIFORM_FLOAT);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginShaderMode(shader);
        DrawRectangle(0, 0, w, h, WHITE);
        EndShaderMode();

        EndDrawing();
    }

    CloseWindow();
}