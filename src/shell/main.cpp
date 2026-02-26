/*
 * VOS — Desktop Shell
 * Main entry point for the desktop virtual OS environment.
 * Uses SDL2 + OpenGL3 + ImGui for the UI.
 */

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/kernel.h"
#include "core/vfs.h"
#include "core/privacy.h"
#include "core/mesh_net.h"
#include "core/lockdown.h"
#include "core/crypto.h"
#include "core/settings.h"
#include "core/dns_guard.h"
#include "core/event_logger.h"
#include "core/notifications.h"
#include "apps/dialer.h"
#include "apps/sms.h"
#include "apps/camera.h"
#include "vos/log.h"

using namespace vos;

// ─── Global System Objects ───────────────────────────────────
static Kernel               g_kernel;
static VirtualFS            g_vfs;
static Crypto               g_crypto;
static PrivacyEngine        g_privacy;
static MeshNet              g_mesh;
static LockdownManager      g_lockdown;
static Settings             g_settings;
static DNSGuard             g_dns;
static EventLogger          g_events;
static NotificationManager  g_notify;
static Dialer               g_dialer;
static SmsApp               g_sms;
static CameraApp            g_camera;
static bool                 g_boot_done = false;
static float                g_boot_timer = 0.0f;

// ─── UI State ────────────────────────────────────────────────
static bool g_show_dialer  = false;
static bool g_show_sms     = false;
static bool g_show_camera  = false;
static bool g_show_system  = false;
static bool g_show_events  = false;
static bool g_show_lockdown_picker = false;

static char g_dial_number[32]  = "";
static char g_sms_input[512]   = "";
static std::string g_active_peer;
static int g_lockdown_minutes  = 5;

// ─── Custom VOS Theme ────────────────────────────────────────
static void apply_vos_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(8, 6);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.10f, 0.35f, 0.60f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.15f, 0.40f, 0.65f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.20f, 0.50f, 0.75f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.10f, 0.30f, 0.55f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.15f, 0.35f, 0.55f, 0.80f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.20f, 0.45f, 0.65f, 0.80f);
    c[ImGuiCol_Separator]       = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_Text]            = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.45f, 0.50f, 0.55f, 1.00f);
}

// ─── Status Bar ──────────────────────────────────────────────
static void render_status_bar(float display_w) {
    auto identity  = g_privacy.get_current_identity();
    auto remaining = g_lockdown.get_remaining_time();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(display_w, 32));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 1.0f));
    ImGui::Begin("##StatusBar", nullptr, flags);

    // Left: VOS identity
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "VOS");
    ImGui::SameLine();
    ImGui::Text("| IP: %s", identity.virtual_ip.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("MAC: %s", identity.virtual_mac.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(#%llu)", (unsigned long long)identity.rotation_count);

    // Middle: peers
    ImGui::SameLine(display_w * 0.45f);
    auto peers = g_mesh.get_peers();
    ImGui::Text("Peers: %zu", peers.size());

    // Right: lockdown + unread
    ImGui::SameLine(display_w - 280);
    int unread = g_sms.total_unread();
    if (unread > 0) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[%d MSG]", unread);
        ImGui::SameLine();
    }

    if (g_lockdown.is_active()) {
        long long secs = (long long)remaining.count();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                           "LOCKED %02lld:%02lld", secs / 60, secs % 60);
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "UNLOCKED");
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

// ─── Desktop Launcher ────────────────────────────────────────
static void render_desktop(float display_w, float display_h) {
    ImGui::SetNextWindowPos(ImVec2(0, 32));
    ImGui::SetNextWindowSize(ImVec2(display_w, display_h - 32));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 1.0f));
    ImGui::Begin("##Desktop", nullptr, flags);

    ImGui::SetCursorPos(ImVec2(30, 40));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    // App icons as large buttons
    auto app_button = [&](const char* label, const char* icon, AppId id, bool* show) {
        bool allowed = g_lockdown.is_app_allowed(id);

        if (!allowed) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.1f, 0.1f, 0.8f));
        }

        ImGui::BeginGroup();
        if (ImGui::Button(icon, ImVec2(100, 80))) {
            if (allowed) *show = true;
        }
        float text_w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (100 - text_w) * 0.5f);
        ImGui::Text("%s", label);
        if (!allowed) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX());
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 0.7f), "[LOCKED]");
        }
        ImGui::EndGroup();

        if (!allowed) ImGui::PopStyleColor(2);
    };

    app_button("Phone",   "CALL",  APP_DIALER, &g_show_dialer);
    ImGui::SameLine(0, 30);
    app_button("Messages","SMS",   APP_SMS,    &g_show_sms);
    ImGui::SameLine(0, 30);
    app_button("Camera",  "CAM",   APP_CAMERA, &g_show_camera);
    ImGui::SameLine(0, 30);

    // System info (always available)
    ImGui::BeginGroup();
    if (ImGui::Button("SYS", ImVec2(100, 80))) g_show_system = true;
    float tw = ImGui::CalcTextSize("System").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (100 - tw) * 0.5f);
    ImGui::Text("System");
    ImGui::EndGroup();

    ImGui::PopStyleVar(2);

    // Bottom: lockdown control
    ImGui::SetCursorPos(ImVec2(30, display_h - 32 - 80));
    if (!g_lockdown.is_active()) {
        ImGui::Text("Lockdown Mode:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("##mins", &g_lockdown_minutes, 1, 120, "%d min");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("ACTIVATE LOCKDOWN", ImVec2(180, 30))) {
            g_lockdown.start(Seconds(g_lockdown_minutes * 60));
        }
        ImGui::PopStyleColor();
    } else {
        auto rem = g_lockdown.get_remaining_time();
        long long s = (long long)rem.count();
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1.0f),
                           "LOCKDOWN ACTIVE — %02lld:%02lld remaining. Only Phone, SMS, Camera available.",
                           s / 60, s % 60);
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

// ─── Dialer Window ───────────────────────────────────────────
static void render_dialer() {
    if (!g_show_dialer) return;

    ImGui::SetNextWindowSize(ImVec2(320, 450), ImGuiCond_FirstUseEver);
    ImGui::Begin("Phone", &g_show_dialer);

    auto state = g_dialer.get_state();

    if (state == CallState::IDLE) {
        ImGui::Text("Enter number:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##number", g_dial_number, sizeof(g_dial_number));

        // Keypad
        ImGui::Spacing();
        const char* keys[] = {"1","2","3","4","5","6","7","8","9","*","0","#"};
        for (int i = 0; i < 12; i++) {
            if (i % 3 != 0) ImGui::SameLine();
            if (ImGui::Button(keys[i], ImVec2(60, 45))) {
                size_t len = strlen(g_dial_number);
                if (len < sizeof(g_dial_number) - 1) {
                    g_dial_number[len] = keys[i][0];
                    g_dial_number[len + 1] = '\0';
                }
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.65f, 0.2f, 1.0f));
        if (ImGui::Button("CALL", ImVec2(-1, 45))) {
            g_dialer.dial(g_dial_number);
        }
        ImGui::PopStyleColor();

        // Call history
        ImGui::Separator();
        ImGui::Text("Recent Calls:");
        auto& hist = g_dialer.get_history();
        for (int i = (int)hist.size() - 1; i >= 0 && i >= (int)hist.size() - 10; i--) {
            ImGui::BulletText("%s %s", hist[i].outgoing ? "->" : "<-",
                              hist[i].number.c_str());
        }

    } else {
        // Active call
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Calling: %s",
                           g_dialer.get_current_number().c_str());
        
        if (state == CallState::DIALING)
            ImGui::Text("Connecting...");
        else if (state == CallState::RINGING)
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Ringing...");
        else if (state == CallState::IN_CALL) {
            int dur = g_dialer.get_call_duration();
            ImGui::Text("In Call — %02d:%02d", dur / 60, dur % 60);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("HANG UP", ImVec2(-1, 50))) {
            g_dialer.hang_up();
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

// ─── SMS Window ──────────────────────────────────────────────
static void render_sms() {
    if (!g_show_sms) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Messages", &g_show_sms);

    // Left panel: conversations / peers
    ImGui::BeginChild("##Contacts", ImVec2(140, 0), true);
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Peers");
    ImGui::Separator();

    auto peers = g_mesh.get_peers();
    for (auto& p : peers) {
        bool selected = (g_active_peer == p.peer_id);
        auto* conv = g_sms.get_conversation(p.peer_id);
        char label[128];
        if (conv && conv->unread_count > 0) {
            snprintf(label, sizeof(label), "%s (%d)", p.peer_id.c_str(), conv->unread_count);
        } else {
            snprintf(label, sizeof(label), "%s", p.peer_id.c_str());
        }
        if (ImGui::Selectable(label, selected)) {
            g_active_peer = p.peer_id;
            g_sms.mark_read(p.peer_id);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: chat
    ImGui::BeginChild("##Chat");
    if (!g_active_peer.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Chat with %s",
                           g_active_peer.c_str());
        ImGui::Separator();

        // Message history
        ImGui::BeginChild("##MsgHistory", ImVec2(0, -35), true);
        auto* conv = g_sms.get_conversation(g_active_peer);
        if (conv) {
            for (auto& msg : conv->messages) {
                if (msg.outgoing) {
                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "You: %s",
                                       msg.text.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "%s: %s",
                                       msg.peer_id.c_str(), msg.text.c_str());
                }
            }
            // Auto-scroll
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        // Input
        ImGui::SetNextItemWidth(-70);
        bool enter = ImGui::InputText("##msginput", g_sms_input, sizeof(g_sms_input),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Send", ImVec2(-1, 0)) || enter) {
            if (g_sms_input[0] != '\0') {
                g_sms.send(g_active_peer, g_sms_input);
                g_mesh.send_text(g_active_peer, g_sms_input);
                g_sms_input[0] = '\0';
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
    } else {
        ImGui::TextDisabled("Select a peer to start chatting");
    }
    ImGui::EndChild();

    ImGui::End();
}

// ─── Camera Window ───────────────────────────────────────────
static void render_camera() {
    if (!g_show_camera) return;

    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Camera", &g_show_camera);

    if (!g_camera.is_open()) {
        ImGui::Text("Camera is off");
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.65f, 1.0f));
        if (ImGui::Button("Open Camera", ImVec2(-1, 40))) {
            g_camera.open();
        }
        ImGui::PopStyleColor();
    } else {
        // Simulated viewfinder
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[LIVE VIEWFINDER]");
        ImGui::BeginChild("##Viewfinder", ImVec2(-1, 200), true);
        ImGui::TextWrapped("Camera feed is active. In production this would show the "
                           "webcam stream via SDL2 texture rendering.");
        ImGui::EndChild();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("CAPTURE", ImVec2(-1, 40))) {
            g_camera.capture();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (ImGui::Button("Close Camera", ImVec2(120, 40))) {
            g_camera.close();
        }

        // Gallery
        ImGui::Separator();
        ImGui::Text("Gallery (%zu photos)", g_camera.capture_count());
        auto& gallery = g_camera.get_gallery();
        for (int i = (int)gallery.size() - 1; i >= 0; i--) {
            ImGui::BulletText("%s (%dx%d)", gallery[i].filename.c_str(),
                              gallery[i].width, gallery[i].height);
        }
    }

    ImGui::End();
}

// ─── System Info Window ──────────────────────────────────────
static void render_system_info() {
    if (!g_show_system) return;

    ImGui::SetNextWindowSize(ImVec2(480, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("System Info", &g_show_system);

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "VOS — Virtual OS v0.3.0");
    ImGui::Separator();

    auto id = g_privacy.get_current_identity();
    if (ImGui::CollapsingHeader("Privacy Engine", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Virtual IP:  %s", id.virtual_ip.c_str());
        ImGui::BulletText("Virtual MAC: %s", id.virtual_mac.c_str());
        ImGui::BulletText("Rotations:   %llu", (unsigned long long)id.rotation_count);
        if (ImGui::Button("Force Rotate")) {
            g_privacy.force_rotate();
            g_events.security("Privacy", "Manual IP/MAC rotation triggered");
            g_notify.info("Identity rotated");
        }
    }

    if (ImGui::CollapsingHeader("DNS Guard")) {
        auto stats = g_dns.get_stats();
        ImGui::BulletText("Status: %s", g_dns.is_active() ? "ACTIVE" : "OFF");
        ImGui::BulletText("Queries: %llu total", (unsigned long long)stats.queries_total);
        ImGui::BulletText("Blocked: %llu", (unsigned long long)stats.queries_blocked);
        ImGui::BulletText("Resolved: %llu", (unsigned long long)stats.queries_resolved);
    }

    if (ImGui::CollapsingHeader("Kernel")) {
        auto procs = g_kernel.list_processes();
        ImGui::Text("Active processes: %zu", procs.size());
        for (auto& p : procs) ImGui::BulletText("[%u] %s", p.pid, p.name.c_str());
    }

    if (ImGui::CollapsingHeader("Virtual Filesystem")) {
        ImGui::Text("Files: %zu  |  Size: %zu bytes", g_vfs.total_files(), g_vfs.total_size());
    }

    if (ImGui::CollapsingHeader("Mesh Network")) {
        ImGui::Text("Peer ID: %s", g_mesh.get_own_id().c_str());
        auto peers = g_mesh.get_peers();
        ImGui::Text("Discovered peers: %zu", peers.size());
        for (auto& p : peers) ImGui::BulletText("%s @ %s", p.peer_id.c_str(), p.address.c_str());
    }

    if (ImGui::CollapsingHeader("Lockdown")) {
        if (g_lockdown.is_active()) {
            auto rem = g_lockdown.get_remaining_time();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1.0f), "ACTIVE — %llds remaining", (long long)rem.count());
        } else {
            ImGui::Text("Inactive");
        }
    }

    if (ImGui::CollapsingHeader("Event Log")) {
        auto events = g_events.get_recent(20);
        ImGui::Text("Total events: %zu", g_events.total_events());
        ImGui::BeginChild("##evlog", ImVec2(0, 150), true);
        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            ImVec4 col = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            if (it->severity == EventSeverity::WARNING)  col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            if (it->severity == EventSeverity::SECURITY) col = ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
            if (it->severity == EventSeverity::CRITICAL) col = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
            ImGui::TextColored(col, "[%s] %s", it->source.c_str(), it->message.c_str());
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// ─── Notification Overlay ────────────────────────────────────
static void render_notifications(float dw) {
    auto active = g_notify.get_active();
    float y = 40;
    for (auto& n : active) {
        ImVec4 bg;
        switch (n.type) {
            case NotificationType::SUCCESS:  bg = ImVec4(0.1f, 0.4f, 0.15f, 0.9f); break;
            case NotificationType::WARNING:  bg = ImVec4(0.5f, 0.35f, 0.05f, 0.9f); break;
            case NotificationType::ERROR:    bg = ImVec4(0.5f, 0.1f, 0.1f, 0.9f); break;
            case NotificationType::SECURITY: bg = ImVec4(0.6f, 0.05f, 0.05f, 0.95f); break;
            default:                         bg = ImVec4(0.12f, 0.22f, 0.4f, 0.9f); break;
        }
        ImGui::SetNextWindowPos(ImVec2(dw - 310, y));
        ImGui::SetNextWindowSize(ImVec2(300, 50));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
        char wid[32]; snprintf(wid, 32, "##notif%u", n.id);
        ImGui::Begin(wid, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(ImVec4(1,1,1,1), "%s", n.title.c_str());
        ImGui::TextWrapped("%s", n.message.c_str());
        ImGui::End();
        ImGui::PopStyleColor();
        y += 55;
    }
}

// ─── Boot Splash ─────────────────────────────────────────────
static bool render_boot_splash(float dw, float dh, float dt) {
    g_boot_timer += dt;
    float alpha = 1.0f;
    if (g_boot_timer > 2.5f) alpha = 1.0f - (g_boot_timer - 2.5f) / 0.5f;
    if (alpha <= 0.0f) return true; // done

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(dw, dh));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.04f, alpha));
    ImGui::Begin("##Boot", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    ImVec2 center(dw * 0.5f, dh * 0.4f);
    float pulse = 0.7f + 0.3f * sinf(g_boot_timer * 3.0f);
    ImGui::SetCursorPos(ImVec2(center.x - 60, center.y - 30));
    ImGui::TextColored(ImVec4(0.3f * pulse, 0.7f * pulse, 1.0f * pulse, alpha), "V O S");
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y + 20));
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, alpha), "Virtual Operating System");

    ImGui::SetCursorPos(ImVec2(center.x - 80, center.y + 60));
    const char* steps[] = {"Initializing kernel...", "Starting privacy engine...",
                           "Scanning mesh network...", "Loading apps...", "Ready."};
    int step = (int)(g_boot_timer / 0.6f);
    if (step > 4) step = 4;
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, alpha), "%s", steps[step]);

    ImGui::End();
    ImGui::PopStyleColor();
    return false;
}

// ─── Main ────────────────────────────────────────────────────
int main(int argc, char** argv) {
    // SDL init
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags wflags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
                                               | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("VOS - Virtual OS",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1024, 700, wflags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); // VSync

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    apply_vos_theme();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ── Initialize VOS Core ──
    g_settings.init();
    g_crypto.init();
    g_kernel.init();
    g_vfs.init();
    g_events.init();
    g_notify.init();
    g_dns.init();
    g_privacy.init(g_settings.get_int(Settings::KEY_IP_ROTATION_INTERVAL, 10));
    g_mesh.init(&g_crypto, (uint16_t)g_settings.get_int(Settings::KEY_MESH_PORT, 5055));
    g_mesh.start_discovery();
    g_lockdown.init();
    g_dialer.init();
    g_sms.init();
    g_camera.init();

    // Wire mesh → SMS + event log
    g_mesh.on_message([](const std::string& peer_id, const ByteBuffer& payload) {
        std::string text(payload.begin(), payload.end());
        g_sms.receive(peer_id, text);
        g_events.info("Mesh", "Message from " + peer_id);
    });
    g_mesh.on_peer_found([](const MeshPeer& p) {
        g_events.info("Mesh", "Discovered peer: " + p.peer_id);
        g_notify.info("Peer found: " + p.peer_id);
    });
    g_privacy.on_identity_changed([](const IdentityState& s) {
        g_events.info("Privacy", "Identity rotated to " + s.virtual_ip);
    });

    g_events.security("System", "VOS Desktop started");
    log::info("MAIN", "VOS Desktop started successfully");

    // ── Main Loop ──
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE
                && ev.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        // Tick subsystems
        g_kernel.tick();
        g_dialer.tick();
        g_notify.tick();

        // Render
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        float dw = io.DisplaySize.x;
        float dh = io.DisplaySize.y;
        float dt = io.DeltaTime;

        if (!g_boot_done) {
            g_boot_done = render_boot_splash(dw, dh, dt);
        } else {
            render_status_bar(dw);
            render_desktop(dw, dh);
            render_dialer();
            render_sms();
            render_camera();
            render_system_info();
            render_notifications(dw);
        }

        ImGui::Render();
        glViewport(0, 0, (int)dw, (int)dh);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ── Cleanup ──
    g_events.security("System", "VOS Desktop shutting down");
    g_camera.close();
    g_dns.shutdown();
    g_mesh.shutdown();
    g_privacy.shutdown();
    g_kernel.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    log::info("MAIN", "VOS Desktop shutdown complete");
    return 0;
}
