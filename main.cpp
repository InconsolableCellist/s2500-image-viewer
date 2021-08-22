#include <vector>
#include <glad/glad.h>
#include <SDL.h>
#include "imgui-style.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"

//#include "functions.h"

int windowWidth = 1030;
int windowHeight = 1265;

// 580x1157

const int SCREEN_WIDTH = 768;
const int SCREEN_HEIGHT = 1024;
const int SOURCE_WIDTH = 567;
const int SOURCE_HEIGHT = 1134;

uint32_t* gPixels = NULL;

void SetGLAttributes();
void setupTexture(GLuint *glTexture, uint32_t *pixels);
void HandleEvent(SDL_Event *event, bool *shouldQuit);

void Quit(SDL_Window *window, SDL_GLContext glContext);

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_WindowFlags windowFlags;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_GLContext glContext;
    SDL_Rect stretchRect;

    stretchRect.x = 0;
    stretchRect.y = 0;
    stretchRect.w = SCREEN_WIDTH;
    stretchRect.h = SCREEN_HEIGHT;

    gPixels = (uint32_t*)malloc((SOURCE_WIDTH * SOURCE_HEIGHT * 3));
    memset(gPixels, 255, SOURCE_WIDTH * SOURCE_HEIGHT * 3);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("[ERROR] %s\n", SDL_GetError());
        return -1;
    }

    SetGLAttributes();

    windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL
        | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_ALLOW_HIGHDPI);

    window = SDL_CreateWindow(
        "S-2500 Capture",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        windowFlags);

//    renderer = SDL_GetRenderer(window);
//    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, SOURCE_WIDTH, SOURCE_HEIGHT);
//    SDL_UpdateTexture(texture, NULL, gPixels, SOURCE_WIDTH * sizeof(uint32_t));

    SDL_SetWindowMinimumSize(window, 500, 300);
    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        printf("[ERROR] Couldn't initialize glad\n");
    } else {
        printf("[INFO] glad initialized.\n");
    }

    glViewport(0, 0, windowWidth, windowHeight);

    GLuint glTexture;
    setupTexture(&glTexture, gPixels);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
//    (void)io; // wut

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImVec4 background = ImVec4(35/255.0f, 35/255.0f, 35/255.0f, 1.00f);

    glClearColor(background.x, background.y, background.z, background.w);
    bool shouldQuit = false;

    while (!shouldQuit) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleEvent(&event, &shouldQuit);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        {
            static int counter = 0;
            int sdl_width = 0;
            int sdl_height = 0;
            int controls_width = 0;
            controls_width = sdl_width;

            if ((controls_width /= 3) < 300) { controls_width = 300; }
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowSize(
                ImVec2(static_cast<float>(controls_width), static_cast<float>(sdl_height - 20)),
                ImGuiCond_Always);

            ImGui::Begin("Status", NULL, ImGuiWindowFlags_NoResize);
                ImGui::Dummy(ImVec2(0.0f, 1.0f));
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Last Row");
                ImGui::Text("Scan mode:");
                ImGui::Text("Pulse(s):");
                ImGui::Text("Row(s):");
            ImGui::End();

            ImGui::Begin("Firmware", NULL, ImGuiWindowFlags_NoResize);
                if (ImGui::Button("Scan Rapid")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan Half")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan 3/4")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan Photo")) { printf("Counter button clicked.\n"); }
            ImGui::End();

            ImGui::Begin("Capture", NULL, ImGuiWindowFlags_NoResize);
                if (ImGui::Button("Save next frame")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Begin stacked capture")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("End stacked capture")) { printf("Counter button clicked.\n"); }
            ImGui::End();

            ImGui::Begin("Live output", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Image((void*)(intptr_t)glTexture, ImVec2(SOURCE_WIDTH, SOURCE_HEIGHT));
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    Quit(window, glContext);

    return 0;
}

void Quit(SDL_Window *window, SDL_GLContext glContext) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    free(gPixels);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void HandleEvent(SDL_Event *event, bool *shouldQuit) {
    ImGui_ImplSDL2_ProcessEvent(event);

    switch (event->type) {
        case SDL_QUIT:
            *shouldQuit = true;
            break;
        case SDL_WINDOWEVENT:
            switch(event->window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    windowWidth = event->window.data1;
                    windowHeight = event->window.data2;
                    glViewport(0, 0, windowWidth, windowHeight);
                    break;
                default:
                    break;
            }
            break;
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case SDLK_ESCAPE:
                    *shouldQuit = true;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void SetGLAttributes() {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
}

void setupTexture(GLuint *glTexture, uint32_t *pixels) {
    glGenTextures(1, glTexture);
    glBindTexture(GL_TEXTURE_2D, *glTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SOURCE_WIDTH, SOURCE_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

}
