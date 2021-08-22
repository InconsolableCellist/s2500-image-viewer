#include <vector>
#include <glad/glad.h>
#include <SDL.h>
#include <fcntl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctime>
#include "imgui-style.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#include "sem_capture_info.h"
#include "sem_capture_pixels.h"
#include <sys/time.h>

#define MAX_ADC_VAL 8192

const char *DATA_FILE = "../data.dat";
//const char *DATA_FILE = "/dev/ttyACM1";

int windowWidth = 1030;
int windowHeight = 1265;

void SetGLAttributes();
void setupTexture(GLuint *glTexture, uint8_t *pixels, SEMCapture *capture);
void HandleEvent(SDL_Event *event, bool *shouldQuit);
void Quit(SDL_Window *window, SDL_GLContext &glContext, uint8_t *pixels);
void CreateWindow(SDL_WindowFlags &windowFlags, SDL_Window *&window, SDL_GLContext &glContext);
bool InitSEMCapture(SEMCapture *ci, const char *dataFilePath);
void DeleteSEMCapture(SEMCapture *ci);
void ParseSEMCaptureData(SEMCapture *ci, SEMCapturePixels *p, int bytesRead);
void ParseStatusBytes(SEMCapture *ci, SEMCapturePixels *p, uint16_t &i);

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_WindowFlags windowFlags;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_GLContext glContext;

    uint32_t bytesRead = 0;      // reset every time the buffer is read
    uint32_t totalBytesRead = 0; // total

    SEMCapture capture;
    if (!InitSEMCapture(&capture, DATA_FILE)) {
        printf("Unable to init the SEM capture. Quitting\n");
        return -1;
    }

    SEMCapturePixels capturePixels;
    capturePixels.pixels = (uint8_t*)malloc((capture.sourceWidth * capture.sourceHeight * 4));
    memset(capturePixels.pixels, 0x00, capture.sourceWidth * capture.sourceHeight * 4);

//    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
//        printf("[ERROR] %s\n", SDL_GetError());
//        return -1;
//    }

    SetGLAttributes();

    CreateWindow(windowFlags, window, glContext);

//    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        printf("[ERROR] Couldn't initialize glad\n");
    } else {
        printf("[INFO] glad initialized.\n");
    }

    glViewport(0, 0, windowWidth, windowHeight);

    GLuint glTexture;
    setupTexture(&glTexture, capturePixels.pixels, &capture);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io; // wut

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

        bytesRead = read(capture.datafile, capture.dataBuffer, capture.BUF_SIZEOF_BYTES);
        capture.bytesRead += bytesRead;
        if (bytesRead <= 0) {
            capture.status = CaptureStatus::STATUS_PAUSED;
//            printf("End of data or error. Status: %d. Total read: %d. Errno: %d\n",
//                bytesRead, totalBytesRead, errno);
//            printf("Press any key to exit...\n");
//            getchar();
//            shouldQuit = true;
            // TODO: non-fatal, GUI handling of dead stream
        } else {
            capture.status = CaptureStatus::STATUS_RUNNING;
        }

        ParseSEMCaptureData(&capture, &capturePixels, bytesRead);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, capture.sourceWidth, capture.sourceHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, capturePixels.pixels);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, capture.sourceWidth, capture.sourceHeight, GL_RGBA, GL_UNSIGNED_BYTE, capturePixels.pixels);

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

            ImGui::Begin("Controls");
            if (ImGui::TreeNode("Firmware")) {
                if (ImGui::Button("Scan Rapid")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan Half")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan 3/4")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Scan Photo")) { printf("Counter button clicked.\n"); }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Capture")) {
                if (ImGui::Button("Save next frame")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("Begin stacked capture")) { printf("Counter button clicked.\n"); }
                if (ImGui::Button("End stacked capture")) { printf("Counter button clicked.\n"); }
                ImGui::TreePop();
            }
            ImGui::End();

            ImGui::Begin("Status", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Indent();
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Last Row");
            ImGui::Text(capture.status == CaptureStatus::STATUS_RUNNING ? "Status:\t\tRunning": "Status:\t\tPaused");
            ImGui::Text("Device:\t\t%s", DATA_FILE);
            ImGui::Text("Scan mode:\t\t%d", capture.scanMode);
            ImGui::Text("Pulse Time (s): %f", capture.syncDuration);
            ImGui::Text("Row Time(s):\t%f", capture.frameDuration);
            ImGui::Text("MB captured:\t%f", capture.bytesRead/1e6);
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Text("FPS avg: %.2f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Unindent();
            ImGui::End();

            ImGui::Begin("Live output", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Image((void*)(intptr_t)glTexture, ImVec2(capture.sourceWidth, capture.sourceHeight));
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    DeleteSEMCapture(&capture);
    Quit(window, glContext, capturePixels.pixels);

    return 0;
}

void CreateWindow(SDL_WindowFlags &windowFlags, SDL_Window *&window, SDL_GLContext &glContext) {
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

    SDL_SetWindowMinimumSize(window, 500, 300);
    glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
}

void Quit(SDL_Window *window, SDL_GLContext &glContext, uint8_t *pixels) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    free(pixels);

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

void setupTexture(GLuint *glTexture, uint8_t *pixels, SEMCapture *capture) {
    glGenTextures(1, glTexture);
    glBindTexture(GL_TEXTURE_2D, *glTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, capture->sourceWidth, capture->sourceHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        pixels);

}

/**
 *
 * @param ci
 * @param dataFilePath
 * @return True if init success, false otherwise
 */
bool InitSEMCapture(SEMCapture *ci, const char *dataFilePath) {
    bool succ = true;
    ci->dataBuffer = static_cast<uint16_t *>(malloc(ci->BUF_SIZEOF_BYTES));
    memset(ci->dataBuffer, 0xFF, ci->BUF_SIZEOF_BYTES);

    ci->datafile = open(dataFilePath, O_RDONLY);
    if (ci->datafile == -1) {
        printf("Unable to open file %s!\n", dataFilePath);
        succ = false;
    }

    return succ;
}

void DeleteSEMCapture(SEMCapture *ci) {
    free(ci->dataBuffer);
    if (ci->datafile != -1) {
        close(ci->datafile);
    }
}

void ParseSEMCaptureData(SEMCapture *ci, SEMCapturePixels *p, int bytesRead) {
    uint16_t *buf = ci->dataBuffer;
    double pixelIntensity = 0;
    uint32_t val;
    uint32_t loc;

    for (uint16_t i=0; i<(bytesRead/sizeof(uint16_t)); i++) {
        while (buf[i] == 0xFEFA || buf[i] == 0xFEFB) {
            ParseStatusBytes(ci, p, i);
        }
        if (buf[i] > p->max && buf[i] < MAX_ADC_VAL) {
            p->max = buf[i];
            printf("min/max: %d/%d\n", p->min, p->max);
        }
        if (buf[i] < p->min) {
            p->min = buf[i];
            printf("min/max: %d/%d\n", p->min, p->max);
        }
        if (p->max == 0) {
            p->max = 1;
        }
        if (p->y >= ci->sourceHeight) {
            p->y = 0;
            printf("Frame overflow at byte: %d\n", bytesRead);
        }
        if (p->x < ci->sourceWidth) {
            val = ( ((double)buf[i]) / p->max ) * 255;
            if (val > 255) {
                val = 255;
            }
            loc = ( ( (p->y) * ci->sourceWidth ) + p->x) * 4;
            p->pixels[loc]        = val; // R
            p->pixels[loc + 1]    = val; // G
            p->pixels[loc + 2]    = val; // B
            p->pixels[loc + 3]    = val; // A

//            p->pixels[((p->y * ci->sourceWidth) + (p->x) ) *3]        = val; // R
//            p->pixels[(((p->y * ci->sourceWidth) + (p->x) ) *3) + 1]    = val; // G
//            p->pixels[(((p->y * ci->sourceWidth) + (p->x) ) *3) + 2]    = val; // B
        } else {
            printf("x overflow at: %d\n", p->x);
        }
        p->x += 1;
    }
}

/**
 * Status bytes are sent every X and/or Y pulse
 * @param ci
 * @param p
 * @param totalBytesRead  Reference to the total number of bytes read. Will be incremented
 * @param i Reference to the iterator over the ci->dataBuffer. Will be incremented
 */
void ParseStatusBytes(SEMCapture *ci, SEMCapturePixels *p, uint16_t &i) {
    uint16_t *buf = ci->dataBuffer;
    if (buf[i] == 0xFEFB) {
        ci->newFrame = 1;
    }
    ci->syncDuration    = buf[++i];
    ci->syncDuration   += ((double)buf[++i]) / 1e6;
    ci->scanMode        = buf[++i];
    ci->frameDuration   = buf[++i] / 16;
    ci->frameDuration  += ((double)buf[++i]) / 1e6;

    if (ci->syncDuration > ci->maxSync) {
        ci->maxSync = ci->syncDuration;
    }
    if (ci->syncDuration < ci->minSync) {
        ci->minSync = ci->syncDuration;
    }

    if (ci->newFrame) {
        // This pulse is an X+Y pulse
        ci->newFrame = 0;
        p->x = 0;
        p->y = 0;
    } else {
        // Just an X pulse
//        printf("x pulse\n\tx: %d\n\tscanMode: %d\n\tpulse duration: %f\n\tframe duration: %f\n", p->x, ci->scanMode, ci->syncDuration, ci->frameDuration);
//        printf("\tsyncAverage: %f\n\tmaxSync: %f\n\tminSync: %f\n", ci->syncAverage/ci->syncNum, ci->maxSync, ci->minSync);
        p->x = 0;
        p->y += 1;
    }
    ci->syncNum += 1;
    ci->syncAverage += ci->syncDuration;

    i++;
}
