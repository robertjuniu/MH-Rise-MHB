// SPDX-License-Identifier: GPL-2.0-or-later
#include "imgui.h"
#include "imgui_xeno.h"
#include "imgui_backend/imgui_nvn.h"
#include "renderer_offsets.h"
#include "monster_assets.h"
#include "hud_life_gate.h"
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cmath>

struct HpPacket { float current, maximum; uint64_t target; uint32_t sequence, species; };
extern "C" int mhr_read_hp(HpPacket *);
extern "C" void mhr_read_player_life(uint64_t *, uint64_t *);
extern "C" long long mhr_get_tick();
extern "C" void mhr_log_line(const char *);
extern "C" const unsigned char mhr_icons_start[];
extern "C" const unsigned char mhr_font_regular_start[], mhr_font_regular_end[];
extern "C" const unsigned char mhr_font_bold_start[], mhr_font_bold_end[];
static ImFont *hud_regular, *hud_bold, *hud_numeric;
extern "C" void *nvnBootstrapLoader(const char *);
static nvnImGui::OrigNvnBootstrap original_bootstrap;
static int icon_rects[sizeof(mhr_monsters) / sizeof(mhr_monsters[0])];
static ImVec2 icon_start[sizeof(icon_rects) / sizeof(icon_rects[0])];
static ImVec2 icon_end[sizeof(icon_rects) / sizeof(icon_rects[0])];

static void *bootstrap(const char *name) { return imgui_xeno_bootstrap_hook(name, original_bootstrap); }
static void prepare_atlas() {
    auto *atlas = ImGui::GetIO().Fonts;
    ImFontConfig font;
    font.SizePixels = 20;
    font.FontDataOwnedByAtlas = false;
    font.OversampleH = 2;
    font.OversampleV = 1;
    static const ImWchar glyphs[] = {0x20, 0x7e, 0};
    atlas->TexDesiredWidth = 1024;
    hud_regular = atlas->AddFontFromMemoryTTF((void *)mhr_font_regular_start,
        int(mhr_font_regular_end - mhr_font_regular_start), 20, &font, glyphs);
    hud_bold = atlas->AddFontFromMemoryTTF((void *)mhr_font_bold_start,
        int(mhr_font_bold_end - mhr_font_bold_start), 20, &font, glyphs);
    hud_numeric = atlas->AddFontFromMemoryTTF((void *)mhr_font_bold_start,
        int(mhr_font_bold_end - mhr_font_bold_start), 14, &font, glyphs);
    for (unsigned i = 0; i < sizeof(icon_rects) / sizeof(icon_rects[0]); ++i)
        icon_rects[i] = atlas->AddCustomRectRegular(64, 64);
    atlas->Build();
    unsigned char *pixels;
    int width, height;
    atlas->GetTexDataAsRGBA32(&pixels, &width, &height);
    for (unsigned i = 0; i < sizeof(icon_rects) / sizeof(icon_rects[0]); ++i) {
        const auto *rect = atlas->GetCustomRectByIndex(icon_rects[i]);
        for (unsigned row = 0; row < 64; ++row)
            memcpy(pixels + ((rect->Y + row) * width + rect->X) * 4,
                mhr_icons_start + (i * 64 * 64 + row * 64) * 4, 64 * 4);
        icon_start[i] = ImVec2(float(rect->X) / width, float(rect->Y) / height);
        icon_end[i] = ImVec2(float(rect->X + 64) / width, float(rect->Y + 64) / height);
    }
    mhr_log_line("[MHR-HUD] Roboto Regular/Bold and 78 monster icons prepared");
}
static void ready() {
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.MouseDrawCursor = false;
    io.ConfigFlags = ImGuiConfigFlags_None;
    mhr_log_line("[MHR-HUD] NVN renderer ready");
}
static void draw() {
    static bool announced;
    if (!announced) { mhr_log_line("[MHR-HUD] draw callback running"); announced = true; }
    static uint32_t last_sequence;
    static uint64_t last_target;
    static long long last_hit;
    static HpPacket hp{};
    static float trail;
    static char percentage[32], health[64];
    static MhrHudLifeGate life_gate{};
    HpPacket sample;
    long long now = mhr_get_tick();
    if (mhr_read_hp(&sample) && sample.sequence != last_sequence) {
        trail = sample.target == last_target ? fmaxf(trail, hp.current) : sample.current;
        hp = sample;
        last_target = hp.target;
        last_sequence = hp.sequence;
        last_hit = now;
        snprintf(percentage, sizeof(percentage), "%.1f%%", hp.current / hp.maximum * 100);
        snprintf(health, sizeof(health), "%.0f / %.0f", hp.current, hp.maximum);
    }
    uint64_t faint=0, revive=0;
    mhr_read_player_life(&faint,&revive);
    if (!mhr_life_visible(&life_gate,now,faint,revive,last_sequence) ||
        !last_sequence || hp.current <= 0 || hp.maximum <= 0 || now - last_hit > 30LL * 19200000) return;
    auto &io = ImGui::GetIO();
    float short_side = fminf(io.DisplaySize.x, io.DisplaySize.y);
    constexpr float hud_scale = 1.166f * 1.10f;
    float unit = short_side / 1080.f * hud_scale;
    float width = fminf(io.DisplaySize.x * .28f, short_side * .66f) * hud_scale;
    float left = (io.DisplaySize.x - width) * .5f;
    float top = short_side * .16f;
    float bar_left = left + 56 * unit, right = left + width;
    float bar_top = top + 35 * unit, bar_height = 10 * unit;
    int monster = -1;
    for (unsigned i = 0; i < sizeof(mhr_monsters) / sizeof(mhr_monsters[0]); ++i)
        if (mhr_monsters[i].id == hp.species) { monster = int(i); break; }
    const char *name = monster < 0 ? "Monster" : mhr_monsters[monster].name;
    const char *kind = monster < 0 ? "" : mhr_monsters[monster].kind;
    float name_size = 19 * unit;
    float measured = hud_bold->CalcTextSizeA(name_size, 1000, 0, name).x;
    if (measured > right - bar_left - 64 * unit) name_size *= (right - bar_left - 64 * unit) / measured;
    float percent_size = float(int(14 * unit + .5f));
    float percent_width = hud_numeric->CalcTextSizeA(percent_size, 1000, 0, percentage).x;
    auto *list = ImGui::GetForegroundDrawList();
    // Faint, feathered shadows follow the text, leaving the header gap clear.
    auto soft_backdrop = [list, unit](float x, float y, float w, float h) {
        for (int layer = 0; layer < 3; ++layer) {
            float pad = (4 - 2 * layer) * unit;
            int alpha = layer == 0 ? 12 : layer == 1 ? 18 : 35;
            list->AddRectFilled(ImVec2(x - pad, y - pad), ImVec2(x + w + pad, y + h + pad),
                IM_COL32(8,10,12,alpha), (5 - layer) * unit);
        }
    };
    float name_width = hud_bold->CalcTextSizeA(float(int(name_size + .5f)), 1000, 0, name).x;
    float kind_width = hud_regular->CalcTextSizeA(float(int(11 * unit + .5f)), 1000, 0, kind).x;
    soft_backdrop(bar_left - 2 * unit, top - unit,
        fminf(fmaxf(name_width, kind_width) + 4 * unit, right - bar_left - percent_width - 10 * unit), 33 * unit);
    soft_backdrop(right - percent_width - 2 * unit, top + 15 * unit,
        percent_width + 4 * unit, 17 * unit);
    if (monster >= 0) list->AddImage(io.Fonts->TexID,
        ImVec2(left, top + 13 * unit), ImVec2(left + 48 * unit, top + 61 * unit),
        icon_start[monster], icon_end[monster]);
    auto text = [list, unit](const char *value, float x, float y, float size, ImFont *font) {
        x = float(int(x + .5f));
        y = float(int(y + .5f));
        size = float(int(size + .5f));
        float stroke = fmaxf(.75f, .75f * unit);
        const ImVec2 offsets[] = {ImVec2(-stroke, 0), ImVec2(stroke, 0),
                                 ImVec2(0, -stroke), ImVec2(0, stroke)};
        for (const auto &offset : offsets)
            list->AddText(font, size, ImVec2(x + offset.x, y + offset.y), IM_COL32(0,0,0,235), value);
        list->AddText(font, size, ImVec2(x,y), IM_COL32_WHITE, value);
    };
    text(kind, bar_left, top, 11 * unit, hud_regular);
    text(name, bar_left, top + 12 * unit, name_size, hud_bold);
    text(percentage, right - percent_width, top + 17 * unit, percent_size, hud_numeric);
    list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(right, bar_top + bar_height), IM_COL32(12,12,12,230));
    trail = fmaxf(hp.current, trail - hp.maximum * io.DeltaTime * .02f);
    list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(bar_left + (right-bar_left) * trail/hp.maximum, bar_top+bar_height), IM_COL32(219,138,64,255));
    float fill = bar_left + (right-bar_left) * hp.current/hp.maximum;
    list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(fill, bar_top+bar_height), IM_COL32(203,51,42,255));
    list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(fill, bar_top+2*unit), IM_COL32(240,104,74,255));
    for (int i = 1; i < 10; ++i) {
        float tick = bar_left + (right-bar_left)*i/10;
        list->AddLine(ImVec2(tick-3*unit, bar_top), ImVec2(tick+3*unit, bar_top+bar_height), IM_COL32(0,0,0,85), unit);
    }
    list->AddRect(ImVec2(bar_left, bar_top), ImVec2(right, bar_top+bar_height), IM_COL32(157,150,145,255), 0, 0, unit);
    float health_size = float(int(14 * unit + .5f));
    float health_width = hud_numeric->CalcTextSizeA(health_size, 1000, 0, health).x;
    soft_backdrop(bar_left - 2 * unit, top + 47 * unit,
        health_width + 4 * unit, 17 * unit);
    text(health, bar_left, top + 49 * unit, health_size, hud_numeric);
}
extern "C" int mhr_install_renderer(uintptr_t main_address) {
    auto **slot = reinterpret_cast<void **>(main_address + MHR_NVN_BOOTSTRAP_GOT);
    original_bootstrap = nvnBootstrapLoader;
    if (!original_bootstrap) return 0;
    imgui_xeno_add_on_pre_init(prepare_atlas);
    imgui_xeno_init(ready, draw);
    *slot = reinterpret_cast<void *>(bootstrap);
    mhr_log_line("[MHR-HUD] NVN bootstrap import redirected by addon");
    return 1;
}
