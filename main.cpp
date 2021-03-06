#include <vector>
#include <glad/glad.h>
#include <SDL.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#include "sem_capture_info.h"
#include "sem_capture_pixels.h"
#include <termios.h>
#include <thread>
#include <mutex>
#include "Logger.h"
#include "SequenceWriter.h"

#define MAX_ADC_VAL 8192

// Data source 0 should always be cached data and will be opened in read-only mode.
// The others should be devices and will be opened in RW mode
const char *ttySources[] = { "../data.dat", "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2", "/dev/ttyACM3", "/dev/ttyACM4" };
static int currentTtySource = 0;

#define COMMAND_SCAN_RESTART        0xA0
#define COMMAND_SCAN_RAPID          0xA1
#define COMMAND_SCAN_HALF           0xA2
#define COMMAND_SCAN_HALF_SLOWER    0xA4
#define COMMAND_SCAN_34             0xA4
#define COMMAND_SCAN_34_SLOWER      0xA5
#define COMMAND_SCAN_PHOTO          0xA6
#define COMMAND_HEARTBEAT           0xA7

int windowWidth = 1140;
int windowHeight = 1265;

SequenceWriter *writer = nullptr;

void SetGLAttributes();
void setupTexture(GLuint *glTexture, uint8_t *pixels, SEMCapture *capture);
void HandleEvent(SDL_Event *event, bool *shouldQuit);
void Quit(SDL_Window *window, SDL_GLContext &glContext, uint8_t *pixels);
void CreateWindow(SDL_WindowFlags &windowFlags, SDL_Window *&window, SDL_GLContext &glContext);
bool InitSEMCapture(SEMCapture *ci, const char *dataFilePath, struct termios *termios);
void DeleteSEMCapture(SEMCapture *ci);
void ParseSEMCaptureData(SEMCapture *ci, SEMCapturePixels *p, ssize_t bytesRead);
void ParseStatusBytes(SEMCapture *ci, SEMCapturePixels *p, uint16_t &i);
void SendCommand(uint8_t command, const SEMCapture &capture);
void ImGuiFrame(uint32_t &statusTimer, SEMCapture &capture, SEMCapturePixels &capturePixels, termios &termios, GLuint glTexture,
    std::thread &captureThread, std::mutex &bufferLock, ssize_t &bytesRead, bool &logWindowOpen);
void SetupGLAndImgui(SDL_Window *window, SDL_GLContext glContext, SEMCapturePixels &capturePixels, SEMCapture &capture,
                     GLuint &glTexture);
void GrabBytes(ssize_t &bytesRead, SEMCapture &ci, std::mutex &bufferLock);

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_WindowFlags windowFlags;
    SDL_GLContext glContext;
    GLuint glTexture;
    ssize_t bytesRead = 0;      // reset every time the buffer is read
    uint32_t totalBytesRead = 0; // total
    uint32_t statusTimer = 0;
    std::mutex bufferLock;
    bool logWindowOpen = false;
    int currentSequenceNumber = 0;

    SEMCapture capture;
    struct termios termios;

    Logger::Instance()->init();
    Logger::Instance()->log("Starting up");

    if (!InitSEMCapture(&capture, ttySources[currentTtySource], &termios)) {
        Logger::Instance()->log("Unable to init the SEM capture.");
    }
    capture.shouldCapture = true;
    std::thread captureThread(GrabBytes, std::ref(bytesRead), std::ref(capture), std::ref(bufferLock));

    SEMCapturePixels capturePixels;
    capturePixels.pixels = (uint8_t*)malloc((capture.sourceWidth * capture.sourceHeight * 4));
    memset(capturePixels.pixels, 0x00, capture.sourceWidth * capture.sourceHeight * 4);

    writer = new SequenceWriter(currentSequenceNumber);

    SetGLAttributes();
    CreateWindow(windowFlags, window, glContext);
    SetupGLAndImgui(window, glContext, capturePixels, capture, glTexture);

    bool shouldQuit = false;
    while (!shouldQuit) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleEvent(&event, &shouldQuit);
        }

        if (!capture.bufferReadyForWrite && bufferLock.try_lock()) {
            if (bytesRead <= 0) {
                capture.status = CaptureStatus::STATUS_PAUSED;
//                Logger::Instance()->log("End of data or error. Status: %d. Total read: %d. Errno: %d", bytesRead, totalBytesRead, errno);
            } else {
                capture.status = CaptureStatus::STATUS_RUNNING;
                capture.bytesRead += bytesRead;
            }

            if (capture.status == CaptureStatus::STATUS_RUNNING) {
                ParseSEMCaptureData(&capture, &capturePixels, bytesRead);
            }
            capture.bufferReadyForWrite = true;
            bufferLock.unlock();
        }

        // TODO: render only a region
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, capture.sourceWidth, capture.sourceHeight, GL_RGBA, GL_UNSIGNED_BYTE, capturePixels.pixels);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGuiFrame(statusTimer, capture, capturePixels, termios, glTexture, captureThread, bufferLock, bytesRead, logWindowOpen);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    capture.shouldCapture = false;
    capture.bufferReadyForWrite = false;
    captureThread.join();
    DeleteSEMCapture(&capture);
    Quit(window, glContext, capturePixels.pixels);

    Logger::Instance()->log("Shutting down");
    Logger::Instance()->quit();

    return 0;
}

void SetupGLAndImgui(SDL_Window *window, SDL_GLContext glContext, SEMCapturePixels &capturePixels, SEMCapture &capture,
                     GLuint &glTexture) {
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        Logger::Instance()->log("[ERROR] Couldn't initialize glad");
    } else {
        Logger::Instance()->log("[INFO] glad initialized.");
    }

    glViewport(0, 0, windowWidth, windowHeight);
    setupTexture(&glTexture, capturePixels.pixels, &capture);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io; // wut
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImVec4 background = ImVec4(35/255.0f, 35/255.0f, 35/255.0f, 1.00f);
    glClearColor(background.x, background.y, background.z, background.w);
}

void ImGuiFrame(uint32_t &statusTimer, SEMCapture &capture, SEMCapturePixels &capturePixels, termios &termios, GLuint glTexture,
    std::thread &captureThread, std::mutex &bufferLock, ssize_t &bytesRead, bool &logWindowOpen) {
    ImGui::NewFrame();
    {
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
        ImGui::Text("Scan Speed");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Indent();
            if (ImGui::Button("Restart Scan")) { SendCommand(COMMAND_SCAN_RESTART, capture); }
            if (ImGui::Button("Scan Rapid")) { SendCommand(COMMAND_SCAN_RAPID, capture); }
            if (ImGui::Button("Scan Half")) { SendCommand(COMMAND_SCAN_HALF, capture); }
            if (ImGui::Button("Scan Half Slower")) { SendCommand(COMMAND_SCAN_HALF_SLOWER, capture); }
            if (ImGui::Button("Scan 3/4")) { SendCommand(COMMAND_SCAN_34, capture); }
            if (ImGui::Button("Scan 3/4 Slower")) { SendCommand(COMMAND_SCAN_34_SLOWER, capture); }
            if (ImGui::Button("Scan Photo")) { SendCommand(COMMAND_SCAN_PHOTO, capture); }
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Text("Capture");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Indent();
            if (ImGui::Button("Save next frame")) { Logger::Instance()->log("Counter button clicked."); }
            if (ImGui::Button("Begin stacked capture")) { Logger::Instance()->log("Counter button clicked."); }
            if (ImGui::Button("End stacked capture")) { Logger::Instance()->log("Counter button clicked."); }
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::End();

        ImGui::Begin("Status", NULL, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Indent();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            if (ImGui::Button("Heartbeat")) { SendCommand(COMMAND_HEARTBEAT, capture); }
            if (capture.heartbeat) {
                ImGui::Text("System heartbeat OK!");
                statusTimer += 1;
                if (statusTimer >= 60) {
                    capture.heartbeat = 0;
                    statusTimer = 0;
                }
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::Text(capture.status == STATUS_RUNNING ? "Status:\t\tRunning": "Status:\t\tNo Data");
            ImGui::Text("Device:\t\t%s", ttySources[currentTtySource]);
            ImGui::Combo("", &currentTtySource, ttySources, IM_ARRAYSIZE(ttySources));

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "Last Row");
            ImGui::Text("Scan mode:\t\t%d", capture.scanMode);
            ImGui::Text("Pulse Time (s): %f", capture.syncDuration);
            ImGui::Text("Row Time(s):\t%f", capture.frameDuration);
            ImGui::Text("MB received:\t%f", capture.bytesRead/1e6);
            ImGui::Text("Row overhead (??s):\t%d", capture.lastRowDurationMicroseconds);
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Text("FPS avg: %.2f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button("Restart all")) {
            bufferLock.lock();
            capture.shouldCapture = false;
            capture.bufferReadyForWrite = true;
            bufferLock.unlock();
            captureThread.join();
            InitSEMCapture(&capture, ttySources[currentTtySource], &termios);
            capture.shouldCapture = true;
            captureThread = std::thread(GrabBytes, std::ref(bytesRead), std::ref(capture), std::ref(bufferLock));
        }
        if (ImGui::Button("Reset min/max")) {
            capturePixels.min = MAX_ADC_VAL;
            capturePixels.max = 0;
        }
        ImGui::Checkbox("Show log window", &logWindowOpen);
        ImGui::End();

        ImGui::Begin("Live output", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image((void*)(intptr_t)glTexture, ImVec2(capture.sourceWidth, capture.sourceHeight));
        ImGui::End();

        if (logWindowOpen) {
            ImGui::Begin("Log window", &logWindowOpen);
            ImGui::Text("Log");
            if (ImGui::Button("Close")) {
                logWindowOpen = false;
            }
            ImGui::End();
        }

        ImGui::Begin("Save Captures");
        ImGui::Checkbox("Save frames to disk", &writer->shouldWrite);
        ImGui::Text("Current File Number:\t%d", writer->getCurrentFileNum());
        ImGui::Text("Current Sequence:\t%d", writer->getCurrentSequenceNum());
        ImGui::Text("Current path:\t%s", writer->getCurrentDirectoryName());
        if (ImGui::Button("Next Sequence")) {
            writer->IncrementSequenceNumber();
        }

        ImGui::End();
    }
}

void SendCommand(uint8_t command, const SEMCapture &capture) {
    ssize_t bytesWritten = write(capture.datafile, &command, 1);
    Logger::Instance()->log("num bytes written: %d", bytesWritten);
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
bool InitSEMCapture(SEMCapture *ci, const char *dataFilePath, struct termios *termios) {
    bool succ = true;
    ci->dataBuffer = static_cast<uint16_t *>(malloc(ci->BUF_SIZEOF_BYTES));
    memset(ci->dataBuffer, 0xFF, ci->BUF_SIZEOF_BYTES);

    if (currentTtySource == 0) {
        ci->datafile = open(dataFilePath, O_RDONLY, 0);
    } else {
        ci->datafile = open(dataFilePath, O_RDWR, 0);
    }
    if (ci->datafile == -1) {
        Logger::Instance()->log("Unable to open file %s!", dataFilePath);
        succ = false;
    } else {
        tcflush(ci->datafile, TCIOFLUSH);
    }

    tcgetattr(ci->datafile, termios);
    termios->c_lflag &= ~ICANON;
    termios->c_cc[VTIME] = 10; // 1 second max blocking time
    termios->c_cc[VMIN] = 0;   // return even if no chars (idle serial)
    tcsetattr(ci->datafile, TCSANOW, termios);

    return succ;
}

void DeleteSEMCapture(SEMCapture *ci) {
    free(ci->dataBuffer);
    if (ci->datafile != -1) {
        close(ci->datafile);
    }
}

void ParseSEMCaptureData(SEMCapture *ci, SEMCapturePixels *p, ssize_t bytesRead) {
    uint16_t *buf = ci->dataBuffer;
    double pixelIntensity = 0;
    uint32_t val;
    uint32_t loc;

    auto _rowTimeStart = std::chrono::high_resolution_clock::now();
    for (uint16_t i=0; i<(bytesRead/sizeof(uint16_t)); i++) {
        while (buf[i] == 0xFEFA || buf[i] == 0xFEFB || buf[i] == 0xFEFC) {
            ParseStatusBytes(ci, p, i);
        }
        if (i >= (bytesRead/sizeof(uint16_t))) {
            break;
        }
        if (buf[i] > p->max && buf[i] < MAX_ADC_VAL) {
            p->max = buf[i];
            Logger::Instance()->log("min/max: %d/%d", p->min, p->max);
        }
        if (buf[i] < p->min) {
            p->min = buf[i];
            Logger::Instance()->log("min/max: %d/%d", p->min, p->max);
        }
        if (p->max == 0) {
            p->max = 1;
        }
        if (p->y >= ci->sourceHeight) {
            p->y = 0;
            Logger::Instance()->log("Frame overflow at byte: %d", bytesRead);
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
            Logger::Instance()->log("x overflow at: %d", p->x);
            p->x = 0;
            p->y += 1;
        }
        p->x += 1;
    }
    auto _rowTimeStop = std::chrono::high_resolution_clock::now();
    ci->lastRowDurationMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(_rowTimeStop - _rowTimeStart).count();
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
//        Logger::Instance()->log("New frame");
        ci->newFrame = 1;
    } else if (buf[i] == 0xFEFC) {
        Logger::Instance()->log("Heartbeat!");
        ci->heartbeat = 1;
        i+=6;
        return;
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
        if (writer && writer->shouldWrite) {
            free(writer->saveNextFileInSequence(*ci, *p));
        }
    } else {
        // Just an X pulse
//        Logger::Instance()->log("x pulse\n\tx: %d\n\tscanMode: %d\n\tpulse duration: %f\n\tframe duration: %f", p->x, ci->scanMode, ci->syncDuration, ci->frameDuration);
//        Logger::Instance()->log("\tsyncAverage: %f\n\tmaxSync: %f\n\tminSync: %f", ci->syncAverage/ci->syncNum, ci->maxSync, ci->minSync);
        p->x = 0;
        p->y += 1;
    }
    ci->syncNum += 1;
    ci->syncAverage += ci->syncDuration;

    i++;
}

void GrabBytes(ssize_t &bytesRead, SEMCapture &ci, std::mutex &bufferLock) {
    while (ci.shouldCapture) {
        if (ci.bufferReadyForWrite) {
            bufferLock.lock();
            ci.bufferReadyForWrite = false;
            bytesRead = read(ci.datafile, ci.dataBuffer, ci.BUF_SIZEOF_BYTES);
            bufferLock.unlock();
        }
    }
}
