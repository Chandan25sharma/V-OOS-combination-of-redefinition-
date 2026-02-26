#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <iostream>

#include "core/kernel.h"
#include "core/privacy.h"
#include "core/mesh_net.h"
#include "core/lockdown.h"
#include "core/crypto.h"
#include "vos/log.h"

using namespace vos;

// Global System Objects
static Kernel*          g_kernel = nullptr;
static PrivacyEngine*   g_privacy = nullptr;
static MeshNet*         g_mesh = nullptr;
static LockdownManager* g_lockdown = nullptr;
static Crypto*          g_crypto = nullptr;

// UI State
static bool g_show_dialer = false;
static bool g_show_sms = false;
static bool g_show_camera = false;
static char g_sms_input[256] = "";
static std::string g_active_peer = "";

void render_status_bar() {
    auto identity = g_privacy->get_current_identity();
    auto remaining = g_lockdown->get_remaining_time();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 30));
    ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    
    ImGui::Text("VOS 1.0 | Identity: %s", identity.virtual_ip.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(" (MAC: %s)", identity.virtual_mac.c_str());
    
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 200);
    if (g_lockdown->is_active()) {
        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "LOCKED: %llds", (long long)remaining.count());
    } else {
        ImGui::Text("UNLOCKED");
    }
    
    ImGui::End();
}

void render_desktop() {
    ImGui::SetNextWindowPos(ImVec2(0, 30));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y - 30));
    ImGui::Begin("Desktop", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
    
    ImVec2 btn_sz(120, 120);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    
    if (ImGui::Button("PHONE", btn_sz)) g_show_dialer = true;
    ImGui::SameLine();
    if (ImGui::Button("SMS", btn_sz)) g_show_sms = true;
    ImGui::SameLine();
    if (ImGui::Button("CAMERA", btn_sz)) g_show_camera = true;
    
    ImGui::PopStyleVar();
    
    if (!g_lockdown->is_active()) {
        ImGui::SetCursorPos(ImVec2(20, ImGui::GetIO().DisplaySize.y - 100));
        if (ImGui::Button("ACTIVATE LOCKDOWN (1 min)", ImVec2(200, 40))) {
            g_lockdown->start(Seconds(60));
        }
    }
    
    ImGui::End();
}

void render_apps() {
    if (g_show_dialer) {
        ImGui::Begin("Dialer", &g_show_dialer);
        if (!g_lockdown->is_app_allowed(APP_DIALER)) {
            ImGui::TextColored(ImVec4(1,0,0,1), "APP BLOCKED BY LOCKDOWN");
        } else {
            ImGui::Text("Call Service: Active");
            static char number[32] = "";
            ImGui::InputText("Number", number, 32);
            if (ImGui::Button("CALL", ImVec2(100, 40))) {
                vos::log::info("UI", "Calling %s...", number);
            }
        }
        ImGui::End();
    }

    if (g_show_sms) {
        ImGui::Begin("Messages", &g_show_sms);
        if (!g_lockdown->is_app_allowed(APP_SMS)) {
            ImGui::TextColored(ImVec4(1,0,0,1), "APP BLOCKED BY LOCKDOWN");
        } else {
            auto peers = g_mesh->get_peers();
            ImGui::BeginChild("Peers", ImVec2(150, 0), true);
            for (auto& p : peers) {
                if (ImGui::Selectable(p.peer_id.c_str(), g_active_peer == p.peer_id)) {
                    g_active_peer = p.peer_id;
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Chat", ImVec2(0, 0));
            if (!g_active_peer.empty()) {
                ImGui::Text("Chat with %s", g_active_peer.c_str());
                ImGui::Separator();
                // Chat history here...
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
                ImGui::InputText("##msg", g_sms_input, 256);
                ImGui::SameLine();
                if (ImGui::Button("Send")) {
                    g_mesh->send_text(g_active_peer, g_sms_input);
                    g_sms_input[0] = '\0';
                }
            } else {
                ImGui::Text("Select a peer to chat");
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("VOS - Virtual OS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize VOS Core
    g_crypto = new Crypto(); g_crypto->init();
    g_kernel = new Kernel(); g_kernel->init();
    g_privacy = new PrivacyEngine(); g_privacy->init(10); // Rotate every 10s
    g_mesh = new MeshNet(); g_mesh->init(g_crypto); g_mesh->start_discovery();
    g_lockdown = new LockdownManager(); g_lockdown->init();

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        render_status_bar();
        render_desktop();
        render_apps();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        
        g_kernel->tick();
    }

    // Cleanup
    g_mesh->shutdown();
    g_privacy->shutdown();
    g_kernel->shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_WindowDestroy(window);
    SDL_Quit();

    return 0;
}
