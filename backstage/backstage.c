#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "font_extrabold.h"
#include "font_medium.h"

#define W 1280
#define H 720
#define MAX_CONTROLLERS 16
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Backstage "Home Set": bright living-room channels with strong, readable
 * controller focus.  The shape language nods to console home screens while
 * keeping GHL's own type, acid accent, and stage terminology. */
static const SDL_Color INK       = {28, 35, 46, 255};
static const SDL_Color INK_2     = {42, 51, 65, 255};
static const SDL_Color SURFACE   = {237, 243, 248, 255};
static const SDL_Color SURFACE_2 = {207, 218, 228, 255};
static const SDL_Color PAPER     = {255, 255, 255, 255};
static const SDL_Color MUTED     = {98, 111, 126, 255};
static const SDL_Color DIM       = {166, 178, 189, 255};
static const SDL_Color LIME      = {202, 244, 62, 255};
static const SDL_Color CYAN      = {22, 190, 232, 255};
static const SDL_Color CORAL     = {255, 91, 116, 255};
static const SDL_Color GOLD      = {255, 187, 62, 255};
static const SDL_Color GREEN     = {48, 199, 126, 255};

typedef enum {
    ACT_GAME,
    ACT_DIAGNOSTICS,
    ACT_DESKTOP,
    ACT_GAME_INSTALLER,
    ACT_SYSTEM_INSTALLER,
    ACT_ABOUT,
    ACT_REBOOT,
    ACT_POWEROFF,
    ACT_SHELL
} Action;

typedef struct {
    const char *label;
    const char *detail;
    Action action;
} MenuItem;

typedef struct {
    const char *number;
    const char *name;
    const char *kicker;
    const char *headline;
    const char *description;
    SDL_Color accent;
    const MenuItem *items;
    int count;
} Section;

static const MenuItem play_items[] = {
    {"PLAY", "Clone Hero", ACT_GAME},
    {"RESUME", "Same game, same profile", ACT_GAME},
    {"LINE CHECK", "Display / sound / input", ACT_DIAGNOSTICS},
};
static const MenuItem library_items[] = {
    {"SONG LIBRARY", "Open it in Clone Hero", ACT_GAME},
    {"FILES", "Open Desktop Mode", ACT_DESKTOP},
    {"REPAIR CLONE HERO", "Verify or reinstall the game", ACT_GAME_INSTALLER},
};
static const MenuItem system_items[] = {
    {"CHECK MACHINE", "Hardware and session status", ACT_DIAGNOSTICS},
    {"DESKTOP", "Terminal, files and system tools", ACT_DESKTOP},
    {"INSTALL GHL", "Put GHL on another disk", ACT_SYSTEM_INSTALLER},
    {"ABOUT", "GHL 0.1 Encore", ACT_ABOUT},
};
static const MenuItem power_items[] = {
    {"RESTART", "Reboot GHL", ACT_REBOOT},
    {"POWER OFF", "Shut down safely", ACT_POWEROFF},
    {"TERMINAL", "Leave Backstage", ACT_SHELL},
};

static const Section sections[] = {
    {"A", "PLAY", "01 / PLAY", "CLONE HERO",
     "Press A. That's the whole idea.", LIME, play_items, ARRAY_LEN(play_items)},
    {"B", "SONGS", "02 / SONGS", "YOUR SET LIST",
     "The game, the songs, and where they live.", CYAN, library_items, ARRAY_LEN(library_items)},
    {"C", "TOOLS", "03 / TOOLS", "UNDER THE HOOD",
     "Only the useful bits.", GOLD, system_items, ARRAY_LEN(system_items)},
    {"D", "OFF", "04 / POWER", "CALL IT A NIGHT",
     "Leave cleanly.", CORAL, power_items, ARRAY_LEN(power_items)},
};

typedef enum { VIEW_MAIN, VIEW_DIAGNOSTICS, VIEW_ABOUT, VIEW_CONFIRM } View;

static int section_index;
static int item_index;
static float selection_y;
static bool selection_ready;
static float navigation_focus[ARRAY_LEN(sections)];
static float page_progress = 1.0f;
static int page_direction = 1;
static float modal_progress;
static float confirm_focus;
static View presented_view = VIEW_MAIN;
static float frame_delta = 1.0f / 60.0f;
static bool running = true;
static View view = VIEW_MAIN;
static Action confirm_action = ACT_POWEROFF;
static bool confirm_yes;
static SDL_GameController *controllers[MAX_CONTROLLERS];
static SDL_Joystick *joysticks[MAX_CONTROLLERS];
static int controller_count;
static int axis_horizontal;
static int axis_vertical;

typedef struct {
    bool game;
    bool display;
    bool audio;
    bool network;
    char ip[32];
    char clock[16];
    unsigned long memory_used_mb;
    unsigned long memory_total_mb;
    int disk_used_percent;
    int songs;
} SystemStatus;
static SystemStatus status_info;

typedef struct {
    SDL_Texture *texture;
    stbtt_packedchar chars[95];
    int width, height;
    float ascent;
} Font;

static Font f_logo, f_hero, f_heading, f_item, f_nav, f_body, f_small, f_tiny;
static SDL_Texture *background;

typedef enum { SCALE_FIT, SCALE_STRETCH, SCALE_INTEGER } ScaleMode;
typedef struct {
    SDL_Rect viewport;
    float scale_x;
    float scale_y;
    int output_width;
    int output_height;
    ScaleMode mode;
} DisplayLayout;
static DisplayLayout display_layout;

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float smooth_toward(float value, float target, float speed) {
    float amount = 1.0f - expf(-speed * frame_delta);
    return value + (target - value) * amount;
}

static float ease_out_cubic(float value) {
    float inverse = 1.0f - clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

static SDL_Color mix_color(SDL_Color from, SDL_Color to, float amount) {
    amount = clamp01(amount);
    return (SDL_Color){
        (Uint8)(from.r + (to.r - from.r) * amount),
        (Uint8)(from.g + (to.g - from.g) * amount),
        (Uint8)(from.b + (to.b - from.b) * amount),
        (Uint8)(from.a + (to.a - from.a) * amount),
    };
}

static void offset_viewport(SDL_Renderer *renderer, SDL_Rect *saved, int x, int y) {
    SDL_RenderGetViewport(renderer, saved);
    SDL_Rect shifted = *saved;
    shifted.x += x;
    shifted.y += y;
    SDL_RenderSetViewport(renderer, &shifted);
}

static Font bake_font(SDL_Renderer *renderer, const unsigned char *ttf, float pixels) {
    Font font = {0};
    font.width = 1024;
    font.height = 1024;
    unsigned char *bitmap = calloc(1, (size_t)font.width * font.height);
    stbtt_pack_context pack = {0};
    if (!stbtt_PackBegin(&pack, bitmap, font.width, font.height, 0, 1, NULL)) {
        fprintf(stderr, "backstage: could not create font atlas\n");
        free(bitmap);
        return font;
    }
    stbtt_PackSetOversampling(&pack, 2, 2);
    stbtt_PackFontRange(&pack, ttf, 0, pixels, 32, 95, font.chars);
    stbtt_PackEnd(&pack);

    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf, 0);
    int ascent;
    stbtt_GetFontVMetrics(&info, &ascent, NULL, NULL);
    font.ascent = ascent * stbtt_ScaleForPixelHeight(&info, pixels);

    unsigned char *rgba = malloc((size_t)font.width * font.height * 4);
    for (int i = 0; i < font.width * font.height; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        rgba, font.width, font.height, 32, font.width * 4, SDL_PIXELFORMAT_RGBA32);
    font.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureBlendMode(font.texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surface);
    free(rgba);
    free(bitmap);
    return font;
}

static float text_width(Font *font, const char *string) {
    float x = 0.0f, y = 0.0f;
    for (; *string; string++) {
        if (*string < 32 || *string > 126) continue;
        stbtt_aligned_quad quad;
        stbtt_GetPackedQuad(font->chars, font->width, font->height, *string - 32, &x, &y, &quad, 1);
    }
    return x;
}

static void draw_text(SDL_Renderer *renderer, Font *font, float x, float top,
                      SDL_Color color, const char *string) {
    SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font->texture, color.a);
    float xpos = x, ypos = top + font->ascent;
    for (; *string; string++) {
        if (*string < 32 || *string > 126) continue;
        stbtt_aligned_quad quad;
        stbtt_GetPackedQuad(font->chars, font->width, font->height, *string - 32,
                            &xpos, &ypos, &quad, 1);
        SDL_FRect destination = {quad.x0, quad.y0, quad.x1 - quad.x0, quad.y1 - quad.y0};
        SDL_Rect source = {(int)(quad.s0 * font->width), (int)(quad.t0 * font->height),
                           (int)((quad.s1 - quad.s0) * font->width),
                           (int)((quad.t1 - quad.t0) * font->height)};
        SDL_RenderCopyF(renderer, font->texture, &source, &destination);
    }
}

static void draw_text_centered(SDL_Renderer *renderer, Font *font, float center, float top,
                               SDL_Color color, const char *string) {
    draw_text(renderer, font, center - text_width(font, string) / 2.0f, top, color, string);
}

static void fill(SDL_Renderer *renderer, int x, int y, int width, int height, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rectangle = {x, y, width, height};
    SDL_RenderFillRect(renderer, &rectangle);
}

static void update_display_layout(SDL_Renderer *renderer) {
    int output_width = W, output_height = H;
    SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
    if (output_width < 1) output_width = W;
    if (output_height < 1) output_height = H;

    const char *requested = getenv("BACKSTAGE_SCALE");
    ScaleMode mode = SCALE_FIT;
    if (requested && !strcmp(requested, "stretch")) mode = SCALE_STRETCH;
    else if (requested && !strcmp(requested, "integer")) mode = SCALE_INTEGER;

    float scale_x = output_width / (float)W;
    float scale_y = output_height / (float)H;
    SDL_Rect viewport = {0, 0, output_width, output_height};
    if (mode != SCALE_STRETCH) {
        float scale = scale_x < scale_y ? scale_x : scale_y;
        if (mode == SCALE_INTEGER && scale >= 1.0f) scale = floorf(scale);
        if (scale <= 0.0f) scale = 1.0f;
        viewport.w = (int)floorf(W * scale + 0.5f);
        viewport.h = (int)floorf(H * scale + 0.5f);
        viewport.x = (output_width - viewport.w) / 2;
        viewport.y = (output_height - viewport.h) / 2;
        scale_x = scale_y = scale;
    }
    display_layout = (DisplayLayout){viewport, scale_x, scale_y,
                                     output_width, output_height, mode};
}

static void begin_scaled_frame(SDL_Renderer *renderer) {
    /* Clear the complete monitor first so non-16:9 screens get intentional,
     * centered black bars rather than stale framebuffer pixels. */
    SDL_RenderSetViewport(renderer, NULL);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderDrawColor(renderer, INK.r, INK.g, INK.b, 255);
    SDL_RenderClear(renderer);
    /* SDL applies RenderSetScale to viewport coordinates too. Express the
     * viewport in logical units so the canvas is scaled exactly once. */
    SDL_RenderSetScale(renderer, display_layout.scale_x, display_layout.scale_y);
    SDL_Rect logical_viewport = {
        (int)floorf(display_layout.viewport.x / display_layout.scale_x + 0.5f),
        (int)floorf(display_layout.viewport.y / display_layout.scale_y + 0.5f),
        W,
        H,
    };
    SDL_RenderSetViewport(renderer, &logical_viewport);
}

static void circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -radius; y <= radius; y++) {
        int x = (int)sqrtf((float)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - x, cy + y, cx + x, cy + y);
    }
}

static void circle_outline(SDL_Renderer *renderer, int cx, int cy, int radius,
                           int thickness, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int ring = 0; ring < thickness; ring++) {
        int r = radius - ring;
        for (int degree = 0; degree < 360; degree += 3) {
            float angle = degree * (float)M_PI / 180.0f;
            SDL_RenderDrawPoint(renderer, cx + (int)(cosf(angle) * r),
                                cy + (int)(sinf(angle) * r));
        }
    }
}

static void rounded_box(SDL_Renderer *renderer, int x, int y, int width, int height,
                        int radius, SDL_Color color) {
    if (width <= 0 || height <= 0) return;
    if (radius < 1) {
        fill(renderer, x, y, width, height, color);
        return;
    }
    if (radius * 2 > width) radius = width / 2;
    if (radius * 2 > height) radius = height / 2;
    if (radius < 1) {
        fill(renderer, x, y, width, height, color);
        return;
    }
    fill(renderer, x + radius, y, width - radius * 2, height, color);
    fill(renderer, x, y + radius, width, height - radius * 2, color);
    circle(renderer, x + radius, y + radius, radius, color);
    circle(renderer, x + width - radius - 1, y + radius, radius, color);
    circle(renderer, x + radius, y + height - radius - 1, radius, color);
    circle(renderer, x + width - radius - 1, y + height - radius - 1, radius, color);
}

static void rounded_outline(SDL_Renderer *renderer, int x, int y, int width, int height,
                            int radius, int thickness, SDL_Color color) {
    if (width <= 0 || height <= 0 || radius < 1 || thickness < 1) return;
    for (int line = 0; line < thickness; line++) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, x + radius, y + line,
                           x + width - radius, y + line);
        SDL_RenderDrawLine(renderer, x + radius, y + height - 1 - line,
                           x + width - radius, y + height - 1 - line);
        SDL_RenderDrawLine(renderer, x + line, y + radius,
                           x + line, y + height - radius);
        SDL_RenderDrawLine(renderer, x + width - 1 - line, y + radius,
                           x + width - 1 - line, y + height - radius);
        int r = radius - line;
        if (r < 1) continue;
        for (int degree = 0; degree <= 90; degree += 3) {
            float angle = degree * (float)M_PI / 180.0f;
            int dx = (int)(cosf(angle) * r);
            int dy = (int)(sinf(angle) * r);
            SDL_RenderDrawPoint(renderer, x + width - radius - 1 + dx,
                                y + height - radius - 1 + dy);
            SDL_RenderDrawPoint(renderer, x + radius - dx,
                                y + height - radius - 1 + dy);
            SDL_RenderDrawPoint(renderer, x + radius - dx, y + radius - dy);
            SDL_RenderDrawPoint(renderer, x + width - radius - 1 + dx,
                                y + radius - dy);
        }
    }
}

static void triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
                     SDL_Color color);

static void render_section_icon(SDL_Renderer *renderer, int section, int cx, int cy,
                                SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (section == 0) {
        triangle(renderer, (SDL_FPoint){cx - 9, cy - 15},
                 (SDL_FPoint){cx + 17, cy}, (SDL_FPoint){cx - 9, cy + 15}, color);
    } else if (section == 1) {
        SDL_RenderDrawLine(renderer, cx + 4, cy - 17, cx + 4, cy + 9);
        SDL_RenderDrawLine(renderer, cx + 4, cy - 17, cx + 18, cy - 13);
        SDL_RenderDrawLine(renderer, cx + 18, cy - 13, cx + 18, cy + 4);
        circle(renderer, cx - 2, cy + 13, 7, color);
        circle(renderer, cx + 12, cy + 8, 7, color);
    } else if (section == 2) {
        for (int ox = -1; ox <= 1; ox += 2)
            for (int oy = -1; oy <= 1; oy += 2)
                rounded_box(renderer, cx + ox * 10 - 6, cy + oy * 10 - 6,
                            12, 12, 3, color);
    } else {
        circle_outline(renderer, cx, cy + 3, 17, 4, color);
        fill(renderer, cx - 3, cy - 20, 6, 22, color);
        circle(renderer, cx, cy - 17, 5, color);
    }
}

static void triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
                     SDL_Color color) {
    SDL_Vertex vertices[3] = {{a, color, {0, 0}}, {b, color, {0, 0}}, {c, color, {0, 0}}};
    const int indices[3] = {0, 1, 2};
    SDL_RenderGeometry(renderer, NULL, vertices, 3, indices, 3);
}

static void make_background(SDL_Renderer *renderer) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1, H, 32, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < H; y++) {
        float factor = y / (float)H;
        Uint8 red = (Uint8)(248 - factor * 9);
        Uint8 green = (Uint8)(251 - factor * 7);
        Uint8 blue = (Uint8)(253 - factor * 3);
        Uint32 *row = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
        row[0] = SDL_MapRGBA(surface->format, red, green, blue, 255);
    }
    background = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
}

static void render_background(SDL_Renderer *renderer, float seconds) {
    SDL_Rect destination = {0, 0, W, H};
    SDL_RenderCopy(renderer, background, NULL, &destination);

    /* Slow ambient drift gives the home screen a little living-room motion
     * without making text or controller focus harder to follow. */
    int wave_x = (int)(sinf(seconds * 0.23f) * 18.0f);
    int wave_y = (int)(cosf(seconds * 0.19f) * 10.0f);
    circle(renderer, 42 + wave_x, 670 + wave_y, 250, (SDL_Color){210, 242, 249, 115});
    circle(renderer, 1238 - wave_x, 120 - wave_y, 220, (SDL_Color){230, 237, 244, 150});
    circle_outline(renderer, 1130 + wave_x / 2, 650 - wave_y, 180, 2,
                   (SDL_Color){211, 223, 233, 120});
    SDL_Color pulse = sections[section_index].accent;
    pulse.a = 35 + (Uint8)((sinf(seconds * 1.7f) + 1.0f) * 12.0f);
    circle(renderer, 1160, 610, 92, pulse);
}

static int count_song_entries(void) {
    DIR *directory = opendir("/root/Clone Hero/Songs");
    if (!directory) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)))
        if (entry->d_name[0] != '.') count++;
    closedir(directory);
    return count;
}

static void refresh_status(void) {
    memset(&status_info, 0, sizeof status_info);
    status_info.game = access("/opt/clonehero/clonehero", F_OK) == 0 ||
                       access("/opt/clonehero/Linux - Standalone/clonehero", F_OK) == 0 ||
                       access("/opt/clonehero/Clone Hero/clonehero", F_OK) == 0;
    status_info.display = getenv("DISPLAY") || getenv("WAYLAND_DISPLAY");
    status_info.audio = access("/dev/snd", F_OK) == 0;
    snprintf(status_info.ip, sizeof status_info.ip, "OFFLINE");

    struct ifaddrs *addresses = NULL;
    if (getifaddrs(&addresses) == 0) {
        for (struct ifaddrs *it = addresses; it; it = it->ifa_next) {
            if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET ||
                !strcmp(it->ifa_name, "lo")) continue;
            struct sockaddr_in *address = (struct sockaddr_in *)it->ifa_addr;
            if (inet_ntop(AF_INET, &address->sin_addr, status_info.ip, sizeof status_info.ip)) {
                status_info.network = true;
                break;
            }
        }
        freeifaddrs(addresses);
    }

    struct sysinfo memory;
    if (sysinfo(&memory) == 0) {
        unsigned long long total = (unsigned long long)memory.totalram * memory.mem_unit;
        unsigned long long free_memory = (unsigned long long)memory.freeram * memory.mem_unit;
        status_info.memory_total_mb = (unsigned long)(total / 1024 / 1024);
        status_info.memory_used_mb = (unsigned long)((total - free_memory) / 1024 / 1024);
    }

    struct statvfs disk;
    if (statvfs("/", &disk) == 0 && disk.f_blocks) {
        unsigned long long used = disk.f_blocks - disk.f_bfree;
        status_info.disk_used_percent = (int)(used * 100 / disk.f_blocks);
    }
    status_info.songs = count_song_entries();

    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    strftime(status_info.clock, sizeof status_info.clock, "%H:%M", &local);
}

static void render_header(SDL_Renderer *renderer) {
    SDL_Color accent = sections[section_index].accent;
    fill(renderer, 0, 0, W, 88, (SDL_Color){255, 255, 255, 238});
    fill(renderer, 0, 86, W, 2, SURFACE_2);
    rounded_box(renderer, 32, 18, 76, 48, 24, accent);
    draw_text_centered(renderer, &f_nav, 70, 31, INK, "GHL");
    draw_text(renderer, &f_heading, 126, 21, INK, "BACKSTAGE");
    draw_text(renderer, &f_tiny, 128, 58, MUTED, "HOME  /  ENCORE 0.1");

    char controller_text[32];
    snprintf(controller_text, sizeof controller_text, "%d PAD", controller_count);
    circle(renderer, 1028, 44, 7, controller_count ? GREEN : DIM);
    draw_text(renderer, &f_tiny, 1043, 37, MUTED, controller_text);
    circle(renderer, 1110, 44, 7, status_info.network ? CYAN : DIM);
    draw_text(renderer, &f_tiny, 1125, 37, MUTED, status_info.network ? "ONLINE" : "OFFLINE");
    draw_text(renderer, &f_nav, 1191, 31, INK, status_info.clock);
}

static void render_navigation(SDL_Renderer *renderer) {
    const int card_width = 276, card_gap = 16, start_x = 40;
    for (int i = 0; i < ARRAY_LEN(sections); i++) {
        int x = start_x + i * (card_width + card_gap);
        bool selected = i == section_index;
        float focus = ease_out_cubic(navigation_focus[i]);
        int y = 119 - (int)(11.0f * focus);
        int height = 156 + (int)(22.0f * focus);
        SDL_Color card = mix_color(PAPER, sections[i].accent, focus);
        SDL_Color label = INK;

        rounded_box(renderer, x + 6, y + 8, card_width, height, 20,
                    (SDL_Color){84, 102, 119, (Uint8)(24 + 21 * focus)});
        rounded_box(renderer, x, y, card_width, height, 20, card);
        rounded_outline(renderer, x, y, card_width, height, 20,
                        1 + (int)(3.0f * focus), mix_color(SURFACE_2, INK, focus));

        SDL_Color icon_back = mix_color(SURFACE, (SDL_Color){255, 255, 255, 155}, focus);
        circle(renderer, x + 48, y + 53, 30, icon_back);
        render_section_icon(renderer, i, x + 48, y + 53,
                            mix_color(sections[i].accent, INK, focus));
        draw_text(renderer, &f_tiny, x + 92, y + 27, mix_color(MUTED, INK_2, focus),
                  sections[i].kicker);
        draw_text(renderer, &f_item, x + 92, y + 50, label, sections[i].name);
        draw_text(renderer, &f_tiny, x + 24, y + height - 35,
                  mix_color(DIM, INK_2, focus),
                  selected ? "SELECTED  /  OPEN BELOW" : "LEFT / RIGHT");
        if (selected) {
            rounded_box(renderer, x + card_width - 50, y + height - 47, 28, 28, 14, INK);
            draw_text_centered(renderer, &f_small, x + card_width - 36,
                               y + height - 42, PAPER, ">");
        }
    }
}

static void render_live_status(SDL_Renderer *renderer, int x, int y) {
    char songs[32], disk[32];
    snprintf(songs, sizeof songs, "%d SONG%s", status_info.songs,
             status_info.songs == 1 ? "" : "S");
    snprintf(disk, sizeof disk, "%d%% USED", status_info.disk_used_percent);

    rounded_box(renderer, x + 5, y + 7, 388, 276, 24, (SDL_Color){69, 89, 108, 35});
    rounded_box(renderer, x, y, 388, 276, 24, PAPER);
    rounded_outline(renderer, x, y, 388, 276, 24, 1, SURFACE_2);
    draw_text(renderer, &f_tiny, x + 24, y + 20, MUTED, "SYSTEM AT A GLANCE");
    draw_text(renderer, &f_nav, x + 24, y + 42, INK, "READY FOR THE NEXT SONG");

    const char *labels[] = {"GAME", "AUDIO", "NETWORK", "LIBRARY"};
    const char *values[] = {status_info.game ? "READY" : "INSTALL",
                            status_info.audio ? "READY" : "MISSING",
                            status_info.network ? status_info.ip : "OFFLINE", songs};
    bool good[] = {status_info.game, status_info.audio, status_info.network, true};
    for (int i = 0; i < 4; i++) {
        int col = i % 2, row = i / 2;
        int cell_x = x + 24 + col * 176;
        int cell_y = y + 86 + row * 67;
        rounded_box(renderer, cell_x, cell_y, 164, 55, 14, SURFACE);
        circle(renderer, cell_x + 18, cell_y + 18, 5, good[i] ? GREEN : GOLD);
        draw_text(renderer, &f_tiny, cell_x + 31, cell_y + 10, MUTED, labels[i]);
        draw_text(renderer, &f_small, cell_x + 16, cell_y + 31, INK, values[i]);
    }

    draw_text(renderer, &f_tiny, x + 24, y + 232, MUTED, "STORAGE");
    draw_text(renderer, &f_tiny, x + 310, y + 232, MUTED, disk);
    rounded_box(renderer, x + 24, y + 252, 340, 8, 4, SURFACE_2);
    int used_width = status_info.disk_used_percent * 340 / 100;
    if (used_width > 340) used_width = 340;
    rounded_box(renderer, x + 24, y + 252, used_width, 8, 4,
                status_info.disk_used_percent < 90 ? CYAN : CORAL);
}

static void render_main(SDL_Renderer *renderer) {
    const Section *section = &sections[section_index];
    int main_x = 40;
    draw_text(renderer, &f_tiny, main_x, 319, section->accent, section->kicker);
    draw_text(renderer, &f_heading, main_x, 339, INK, section->headline);
    draw_text(renderer, &f_small, main_x, 379, MUTED, section->description);

    int list_x = main_x, list_y = 411, list_width = 780, item_height = 51, gap = 9;
    float target = (float)(list_y + item_index * (item_height + gap));
    if (!selection_ready) {
        selection_y = target;
        selection_ready = true;
    }
    selection_y = smooth_toward(selection_y, target, 18.0f);
    rounded_box(renderer, list_x + 5, (int)selection_y + 6, list_width, item_height,
                15, (SDL_Color){68, 87, 104, 35});
    rounded_box(renderer, list_x, (int)selection_y, list_width, item_height, 15, section->accent);

    for (int i = 0; i < section->count; i++) {
        int y = list_y + i * (item_height + gap);
        bool selected = i == item_index;
        if (!selected) {
            rounded_box(renderer, list_x, y, list_width, item_height, 15, PAPER);
            rounded_outline(renderer, list_x, y, list_width, item_height, 15, 1, SURFACE_2);
        }
        SDL_Color title = INK;
        SDL_Color detail = selected ? INK_2 : MUTED;
        char index_text[16];
        snprintf(index_text, sizeof index_text, "%02d", i + 1);
        draw_text(renderer, &f_tiny, list_x + 22, y + 19, selected ? INK : DIM, index_text);
        draw_text(renderer, &f_nav, list_x + 68, y + 8, title, section->items[i].label);
        draw_text(renderer, &f_tiny, list_x + 310, y + 19, detail, section->items[i].detail);
        if (selected) {
            rounded_box(renderer, list_x + list_width - 44, y + 11, 30, 30, 15, INK);
            draw_text_centered(renderer, &f_small, list_x + list_width - 29, y + 17, PAPER, ">");
        }
    }

    render_live_status(renderer, 852, 338);
}

static void render_footer(SDL_Renderer *renderer) {
    fill(renderer, 0, 674, W, 46, (SDL_Color){255, 255, 255, 245});
    fill(renderer, 0, 674, W, 1, SURFACE_2);
    rounded_box(renderer, 35, 684, 26, 26, 13, INK);
    draw_text_centered(renderer, &f_tiny, 48, 691, PAPER, "<>");
    draw_text(renderer, &f_small, 72, 689, MUTED, "CHANNEL");
    rounded_box(renderer, 236, 684, 26, 26, 13, sections[section_index].accent);
    draw_text_centered(renderer, &f_tiny, 249, 691, INK, "A");
    draw_text(renderer, &f_small, 273, 689, INK, "SELECT");
    rounded_box(renderer, 398, 684, 26, 26, 13, SURFACE_2);
    draw_text_centered(renderer, &f_tiny, 411, 691, INK, "B");
    draw_text(renderer, &f_small, 435, 689, MUTED, "BACK");
    draw_text(renderer, &f_tiny, 1060, 692, MUTED, "GUITAR / PAD / KEYS");
    draw_text(renderer, &f_tiny, 1210, 692, INK, "0.1");
}

static void diagnostic_row(SDL_Renderer *renderer, int x, int y, int width, const char *label,
                           const char *value, bool good) {
    rounded_box(renderer, x, y, width, 62, 16, SURFACE);
    rounded_outline(renderer, x, y, width, 62, 16, 1, SURFACE_2);
    circle(renderer, x + 24, y + 31, 6, good ? GREEN : GOLD);
    draw_text(renderer, &f_small, x + 44, y + 10, INK, label);
    draw_text(renderer, &f_tiny, x + 44, y + 37, good ? GREEN : GOLD, value);
}

static void render_modal_base(SDL_Renderer *renderer, const char *kicker, const char *title,
                              SDL_Color accent) {
    rounded_box(renderer, 181, 95, 932, 548, 28, (SDL_Color){0, 0, 0, 45});
    rounded_box(renderer, 174, 86, 932, 548, 28, PAPER);
    rounded_outline(renderer, 174, 86, 932, 548, 28, 2, SURFACE_2);
    rounded_box(renderer, 198, 108, 112, 30, 15, accent);
    draw_text_centered(renderer, &f_tiny, 254, 117, INK, kicker);
    draw_text(renderer, &f_heading, 212, 151, INK, title);
    fill(renderer, 216, 204, 848, 1, SURFACE_2);
    rounded_box(renderer, 1035, 108, 42, 42, 21, SURFACE);
    draw_text_centered(renderer, &f_nav, 1056, 118, MUTED, "B");
}

static void render_diagnostics(SDL_Renderer *renderer) {
    render_modal_base(renderer, "PRE-FLIGHT", "SYSTEM CHECK", GOLD);
    const char *scale_name = display_layout.mode == SCALE_STRETCH ? "STRETCH" :
                             display_layout.mode == SCALE_INTEGER ? "INTEGER" : "FIT";
    char display_status[64];
    snprintf(display_status, sizeof display_status, "%d x %d / %s",
             display_layout.output_width, display_layout.output_height, scale_name);
    diagnostic_row(renderer, 216, 228, 400, "CLONE HERO",
                   status_info.game ? "PAYLOAD READY" : "RUN CLONEHERO INSTALL", status_info.game);
    diagnostic_row(renderer, 648, 228, 416, "DISPLAY SESSION",
                   status_info.display ? display_status : "NO DISPLAY", status_info.display);
    diagnostic_row(renderer, 216, 304, 400, "AUDIO OUTPUT",
                   status_info.audio ? "ALSA DEVICE READY" : "NO ALSA DEVICE", status_info.audio);
    diagnostic_row(renderer, 648, 304, 416, "NETWORK",
                   status_info.network ? status_info.ip : "OFFLINE", status_info.network);

    char controller[48], memory[64], disk[48], songs[48];
    snprintf(controller, sizeof controller, "%d CONNECTED", controller_count);
    snprintf(memory, sizeof memory, "%lu / %lu MB USED", status_info.memory_used_mb,
             status_info.memory_total_mb);
    snprintf(disk, sizeof disk, "%d%% OF ROOT DISK USED", status_info.disk_used_percent);
    snprintf(songs, sizeof songs, "%d CONTENT ITEM%s", status_info.songs,
             status_info.songs == 1 ? "" : "S");
    diagnostic_row(renderer, 216, 380, 400, "CONTROLLERS", controller, controller_count > 0);
    diagnostic_row(renderer, 648, 380, 416, "MEMORY", memory, status_info.memory_total_mb > 0);
    diagnostic_row(renderer, 216, 456, 400, "STORAGE", disk, status_info.disk_used_percent < 90);
    diagnostic_row(renderer, 648, 456, 416, "SONG FOLDER", songs, true);

    rounded_box(renderer, 216, 548, 126, 36, 18, GOLD);
    draw_text_centered(renderer, &f_small, 279, 558, INK, "A  REFRESH");
    rounded_box(renderer, 928, 548, 136, 36, 18, SURFACE);
    draw_text_centered(renderer, &f_small, 996, 558, INK, "B  BACK");
}

static void render_about(SDL_Renderer *renderer) {
    render_modal_base(renderer, "FIRST PUBLIC SET", "GUITAR HERO LINUX 0.1", CYAN);
    draw_text(renderer, &f_hero, 216, 229, INK, "THE GUITAR IS THE PC.");
    rounded_box(renderer, 216, 286, 848, 76, 18, SURFACE);
    draw_text(renderer, &f_body, 240, 302, INK,
              "A tiny Linux system built for one job: get from power-on to the first note.");
    draw_text(renderer, &f_small, 240, 334, MUTED,
              "Hand-rolled init. ampkg packages. Backstage shell. Clone Hero runtime.");

    rounded_box(renderer, 216, 391, 260, 100, 18, LIME);
    rounded_box(renderer, 500, 391, 260, 100, 18, SURFACE);
    rounded_box(renderer, 784, 391, 280, 100, 18, CYAN);
    draw_text(renderer, &f_tiny, 240, 413, INK_2, "RELEASE");
    draw_text(renderer, &f_nav, 240, 443, INK, "ENCORE 0.1");
    draw_text(renderer, &f_tiny, 524, 413, MUTED, "KERNEL");
    draw_text(renderer, &f_nav, 524, 443, INK, "LINUX 6.12");
    draw_text(renderer, &f_tiny, 808, 413, INK_2, "GAME SHELL");
    draw_text(renderer, &f_nav, 808, 443, INK, "BACKSTAGE");

    draw_text(renderer, &f_tiny, 216, 548, MUTED,
              "UNOFFICIAL FAN PROJECT / NOT AFFILIATED WITH ACTIVISION OR CLONE HERO");
    rounded_box(renderer, 936, 568, 128, 36, 18, SURFACE);
    draw_text_centered(renderer, &f_small, 1000, 578, INK, "B  BACK");
}

static void render_confirmation(SDL_Renderer *renderer) {
    SDL_Color accent = confirm_action == ACT_REBOOT ? GOLD : CORAL;
    render_modal_base(renderer, "POWER", confirm_action == ACT_REBOOT ? "RESTART GHL?" : "SHUT DOWN?", accent);
    rounded_box(renderer, 216, 226, 848, 72, 18, SURFACE);
    draw_text(renderer, &f_body, 240, 250, MUTED,
              confirm_action == ACT_REBOOT ? "The current session will close and GHL will restart."
                                           : "The current session will close and the system will power off.");
    int cancel_x = 278, confirm_x = 658, button_y = 354, button_w = 300;
    float choice = ease_out_cubic(confirm_focus);
    SDL_Color cancel_fill = mix_color(INK, SURFACE, choice);
    SDL_Color confirm_fill = mix_color(SURFACE, accent, choice);
    rounded_box(renderer, cancel_x, button_y, button_w, 96, 24, cancel_fill);
    rounded_outline(renderer, cancel_x, button_y, button_w, 96, 24, 2,
                    mix_color(INK, SURFACE_2, choice));
    rounded_box(renderer, confirm_x, button_y, button_w, 96, 24, confirm_fill);
    rounded_outline(renderer, confirm_x, button_y, button_w, 96, 24, 2,
                    mix_color(SURFACE_2, accent, choice));
    draw_text_centered(renderer, &f_item, cancel_x + button_w / 2.0f, button_y + 33,
                       mix_color(PAPER, INK, choice), "CANCEL");
    draw_text_centered(renderer, &f_item, confirm_x + button_w / 2.0f, button_y + 33,
                       INK,
                       confirm_action == ACT_REBOOT ? "RESTART" : "POWER OFF");
    draw_text_centered(renderer, &f_small, 640, 522, MUTED,
                       "LEFT / RIGHT TO CHOOSE   /   A TO CONFIRM");
}

static void render(SDL_Renderer *renderer, float seconds) {
    begin_scaled_frame(renderer);
    render_background(renderer, seconds);
    render_header(renderer);
    render_navigation(renderer);
    SDL_Rect base_viewport;
    int page_offset = (int)((1.0f - ease_out_cubic(page_progress)) * 56.0f * page_direction);
    offset_viewport(renderer, &base_viewport, page_offset, 0);
    render_main(renderer);
    SDL_RenderSetViewport(renderer, &base_viewport);
    render_footer(renderer);
    if (presented_view != VIEW_MAIN && modal_progress > 0.001f) {
        fill(renderer, 0, 0, W, H,
             (SDL_Color){20, 29, 39, (Uint8)(178.0f * clamp01(modal_progress))});
        SDL_Rect modal_viewport;
        int modal_offset = (int)((1.0f - ease_out_cubic(modal_progress)) * 46.0f);
        offset_viewport(renderer, &modal_viewport, 0, modal_offset);
        if (presented_view == VIEW_DIAGNOSTICS) render_diagnostics(renderer);
        else if (presented_view == VIEW_ABOUT) render_about(renderer);
        else if (presented_view == VIEW_CONFIRM) render_confirmation(renderer);
        SDL_RenderSetViewport(renderer, &modal_viewport);
    }
}

static void request_session(const char *name) {
    FILE *marker = fopen("/run/ghl-next-session", "w");
    if (marker) {
        fputs(name, marker);
        fclose(marker);
    }
    running = false;
}

static void activate(void) {
    const MenuItem *selected = &sections[section_index].items[item_index];
    switch (selected->action) {
    case ACT_GAME: request_session("clonehero"); break;
    case ACT_DIAGNOSTICS: refresh_status(); view = VIEW_DIAGNOSTICS; break;
    case ACT_DESKTOP: request_session("desktop"); break;
    case ACT_GAME_INSTALLER: request_session("clonehero-install"); break;
    case ACT_SYSTEM_INSTALLER: request_session("installer"); break;
    case ACT_ABOUT: view = VIEW_ABOUT; break;
    case ACT_REBOOT:
    case ACT_POWEROFF:
        confirm_action = selected->action;
        confirm_yes = false;
        view = VIEW_CONFIRM;
        break;
    case ACT_SHELL: running = false; break;
    }
}

static void confirm(void) {
    if (!confirm_yes) {
        view = VIEW_MAIN;
        return;
    }
    if (confirm_action == ACT_REBOOT) execlp("reboot", "reboot", (char *)NULL);
    else execlp("poweroff", "poweroff", (char *)NULL);
}

static void move_section(int direction) {
    section_index = (section_index + direction + ARRAY_LEN(sections)) % ARRAY_LEN(sections);
    item_index = 0;
    selection_ready = false;
    page_direction = direction;
    page_progress = 0.0f;
}

static void move_item(int direction) {
    int count = sections[section_index].count;
    item_index = (item_index + direction + count) % count;
}

static void close_inputs(void) {
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (controllers[i]) SDL_GameControllerClose(controllers[i]);
        if (joysticks[i]) SDL_JoystickClose(joysticks[i]);
        controllers[i] = NULL;
        joysticks[i] = NULL;
    }
    controller_count = 0;
}

static void open_inputs(void) {
    close_inputs();
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count && i < MAX_CONTROLLERS; i++) {
        if (SDL_IsGameController(i)) {
            controllers[i] = SDL_GameControllerOpen(i);
            if (controllers[i]) controller_count++;
        } else {
            joysticks[i] = SDL_JoystickOpen(i);
            if (joysticks[i]) controller_count++;
        }
    }
}

static void back_or_exit(void) {
    if (view != VIEW_MAIN) view = VIEW_MAIN;
    else running = false;
}

static void update_animations(void) {
    for (int i = 0; i < ARRAY_LEN(sections); i++)
        navigation_focus[i] = smooth_toward(navigation_focus[i],
                                            i == section_index ? 1.0f : 0.0f, 13.0f);
    page_progress = smooth_toward(page_progress, 1.0f, 11.0f);
    confirm_focus = smooth_toward(confirm_focus, confirm_yes ? 1.0f : 0.0f, 15.0f);

    if (view != VIEW_MAIN) {
        presented_view = view;
        modal_progress = smooth_toward(modal_progress, 1.0f, 14.0f);
    } else {
        modal_progress = smooth_toward(modal_progress, 0.0f, 16.0f);
        if (modal_progress < 0.002f) {
            modal_progress = 0.0f;
            presented_view = VIEW_MAIN;
        }
    }
}

static void input_direction(int horizontal, int vertical) {
    if (view == VIEW_CONFIRM) {
        if (horizontal) confirm_yes = horizontal > 0;
        return;
    }
    if (view != VIEW_MAIN) return;
    if (horizontal) move_section(horizontal);
    if (vertical) move_item(vertical);
}

static void input_activate(void) {
    if (view == VIEW_MAIN) activate();
    else if (view == VIEW_DIAGNOSTICS) refresh_status();
    else if (view == VIEW_CONFIRM) confirm();
}

static bool logical_pointer(SDL_Window *window, int window_x, int window_y,
                            int *logical_x, int *logical_y) {
    int window_width = display_layout.output_width;
    int window_height = display_layout.output_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    if (window_width < 1 || window_height < 1) return false;
    float output_x = window_x * display_layout.output_width / (float)window_width;
    float output_y = window_y * display_layout.output_height / (float)window_height;
    if (output_x < display_layout.viewport.x || output_y < display_layout.viewport.y ||
        output_x >= display_layout.viewport.x + display_layout.viewport.w ||
        output_y >= display_layout.viewport.y + display_layout.viewport.h)
        return false;
    *logical_x = (int)((output_x - display_layout.viewport.x) / display_layout.scale_x);
    *logical_y = (int)((output_y - display_layout.viewport.y) / display_layout.scale_y);
    return true;
}

static bool inside(int x, int y, int left, int top, int width, int height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

static void pointer_action(SDL_Window *window, int window_x, int window_y, bool click) {
    int x, y;
    if (!logical_pointer(window, window_x, window_y, &x, &y)) return;

    if (view == VIEW_CONFIRM) {
        if (inside(x, y, 278, 354, 300, 96)) {
            confirm_yes = false;
            if (click) confirm();
        } else if (inside(x, y, 658, 354, 300, 96)) {
            confirm_yes = true;
            if (click) confirm();
        }
        return;
    }
    if (view == VIEW_DIAGNOSTICS) {
        if (click && inside(x, y, 216, 548, 126, 36)) refresh_status();
        else if (click && (inside(x, y, 928, 548, 136, 36) ||
                           inside(x, y, 1035, 108, 42, 42)))
            view = VIEW_MAIN;
        return;
    }
    if (view == VIEW_ABOUT) {
        if (click && (inside(x, y, 936, 568, 128, 36) ||
                      inside(x, y, 1035, 108, 42, 42)))
            view = VIEW_MAIN;
        return;
    }
    if (view != VIEW_MAIN) return;

    const int card_width = 276, card_gap = 16, start_x = 40;
    for (int i = 0; i < ARRAY_LEN(sections); i++) {
        int card_x = start_x + i * (card_width + card_gap);
        if (!inside(x, y, card_x, 104, card_width, 184)) continue;
        if (i != section_index) {
            page_direction = i > section_index ? 1 : -1;
            section_index = i;
            item_index = 0;
            selection_ready = false;
            page_progress = 0.0f;
        }
        return;
    }

    const Section *section = &sections[section_index];
    for (int i = 0; i < section->count; i++) {
        int row_y = 411 + i * 60;
        if (!inside(x, y, 40, row_y, 780, 51)) continue;
        item_index = i;
        if (click) activate();
        return;
    }
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--version")) {
        puts("Backstage 0.1 Encore");
        return 0;
    }
    bool screenshot = getenv("BACKSTAGE_SCREENSHOT") != NULL;
    const char *preview = getenv("BACKSTAGE_PREVIEW");
    if (preview) {
        if (!strcmp(preview, "songs")) section_index = 1;
        else if (!strcmp(preview, "tools")) section_index = 2;
        else if (!strcmp(preview, "power")) section_index = 3;
        else if (!strcmp(preview, "diagnostics")) view = VIEW_DIAGNOSTICS;
        else if (!strcmp(preview, "about")) view = VIEW_ABOUT;
        else if (!strcmp(preview, "confirm")) {
            section_index = 3;
            view = VIEW_CONFIRM;
        }
    }
    int requested_width = W, requested_height = H;
    const char *requested_size = getenv("BACKSTAGE_SIZE");
    if (requested_size) {
        int parsed_width = 0, parsed_height = 0;
        if (sscanf(requested_size, "%dx%d", &parsed_width, &parsed_height) == 2 &&
            parsed_width >= 320 && parsed_height >= 240) {
            requested_width = parsed_width;
            requested_height = parsed_height;
        }
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "backstage: %s\n", SDL_GetError());
        return 1;
    }
    if (!screenshot) {
        /* Standalone Xorg has no window manager to honor
         * SDL_WINDOW_FULLSCREEN_DESKTOP. Size the borderless window to the
         * real mode ourselves; Gamescope/Wayland accepts the same dimensions. */
        SDL_DisplayMode desktop_mode;
        if (SDL_GetCurrentDisplayMode(0, &desktop_mode) == 0 &&
            desktop_mode.w >= 320 && desktop_mode.h >= 240) {
            requested_width = desktop_mode.w;
            requested_height = desktop_mode.h;
        }
    }
    open_inputs();
    Uint32 window_flags = screenshot ? SDL_WINDOW_HIDDEN :
        (SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("Backstage", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, requested_width,
                                           requested_height, window_flags);
    if (!window) {
        fprintf(stderr, "backstage: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, screenshot ? SDL_RENDERER_SOFTWARE :
        (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "backstage: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    update_display_layout(renderer);

    f_logo = bake_font(renderer, font_extrabold, 38);
    f_hero = bake_font(renderer, font_extrabold, 46);
    f_heading = bake_font(renderer, font_extrabold, 36);
    f_item = bake_font(renderer, font_extrabold, 22);
    f_nav = bake_font(renderer, font_extrabold, 20);
    f_body = bake_font(renderer, font_medium, 18);
    f_small = bake_font(renderer, font_medium, 16);
    f_tiny = bake_font(renderer, font_medium, 13);
    make_background(renderer);
    refresh_status();

    navigation_focus[section_index] = 1.0f;
    presented_view = view;
    modal_progress = view == VIEW_MAIN ? 0.0f : 1.0f;
    confirm_focus = confirm_yes ? 1.0f : 0.0f;

    render(renderer, 0.0f);
    SDL_RenderPresent(renderer);
    if (screenshot) {
        const char *path = getenv("BACKSTAGE_SCREENSHOT");
        SDL_RenderSetViewport(renderer, NULL);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_Rect capture = {0, 0, display_layout.output_width, display_layout.output_height};
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
            0, capture.w, capture.h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface || SDL_RenderReadPixels(renderer, &capture, SDL_PIXELFORMAT_ARGB8888,
                                             surface->pixels, surface->pitch) != 0 ||
            SDL_SaveBMP(surface, path) != 0) {
            fprintf(stderr, "backstage: screenshot failed: %s\n", SDL_GetError());
            SDL_FreeSurface(surface);
            return 1;
        }
        SDL_FreeSurface(surface);
        return 0;
    }

    Uint32 started = SDL_GetTicks();
    Uint32 previous_frame = started;
    Uint32 last_status = started;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_WINDOWEVENT &&
                     (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                      event.window.event == SDL_WINDOWEVENT_RESIZED))
                update_display_layout(renderer);
            else if (event.type == SDL_CONTROLLERDEVICEADDED ||
                     event.type == SDL_CONTROLLERDEVICEREMOVED ||
                     event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED)
                open_inputs();
            else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) back_or_exit();
                else if (key == SDLK_LEFT || key == SDLK_a || key == SDLK_h)
                    input_direction(-1, 0);
                else if (key == SDLK_RIGHT || key == SDLK_d || key == SDLK_l)
                    input_direction(1, 0);
                else if (key == SDLK_UP || key == SDLK_w || key == SDLK_k)
                    input_direction(0, -1);
                else if (key == SDLK_DOWN || key == SDLK_s || key == SDLK_j)
                    input_direction(0, 1);
                else if (key == SDLK_RETURN || key == SDLK_SPACE) input_activate();
            } else if (event.type == SDL_MOUSEMOTION) {
                pointer_action(window, event.motion.x, event.motion.y, false);
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT)
                    pointer_action(window, event.button.x, event.button.y, true);
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    back_or_exit();
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) input_direction(-1, 0);
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) input_direction(1, 0);
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) input_direction(0, -1);
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) input_direction(0, 1);
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) input_activate();
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) back_or_exit();
            } else if (event.type == SDL_CONTROLLERAXISMOTION &&
                       event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                int direction = event.caxis.value > 16000 ? 1 :
                                event.caxis.value < -16000 ? -1 : 0;
                if (direction && direction != axis_horizontal)
                    input_direction(direction, 0);
                axis_horizontal = direction;
            } else if (event.type == SDL_CONTROLLERAXISMOTION &&
                       event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int direction = event.caxis.value > 16000 ? 1 :
                                event.caxis.value < -16000 ? -1 : 0;
                if (direction && direction != axis_vertical)
                    input_direction(0, direction);
                axis_vertical = direction;
            } else if (event.type == SDL_JOYHATMOTION && event.jhat.value != SDL_HAT_CENTERED) {
                if (event.jhat.value & SDL_HAT_LEFT) input_direction(-1, 0);
                if (event.jhat.value & SDL_HAT_RIGHT) input_direction(1, 0);
                if (event.jhat.value & SDL_HAT_UP) input_direction(0, -1);
                if (event.jhat.value & SDL_HAT_DOWN) input_direction(0, 1);
            } else if (event.type == SDL_JOYBUTTONDOWN) {
                if (event.jbutton.button == 0) input_activate();
                else if (event.jbutton.button == 1) back_or_exit();
            }
        }
        Uint32 now = SDL_GetTicks();
        frame_delta = (now - previous_frame) / 1000.0f;
        if (frame_delta > 0.05f) frame_delta = 0.05f;
        if (frame_delta < 0.001f) frame_delta = 0.001f;
        previous_frame = now;
        if (now - last_status >= 1000) {
            refresh_status();
            last_status = now;
        }
        update_animations();
        render(renderer, (now - started) / 1000.0f);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    close_inputs();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
