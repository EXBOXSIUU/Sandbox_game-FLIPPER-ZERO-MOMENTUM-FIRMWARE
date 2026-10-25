#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GW 64
#define GH 28
#define CELL 2

typedef enum {
    CellEmpty = 0,
    CellSand,
    CellWater,
    CellWall,
} Cell;

typedef enum {
    MatSand = 0,
    MatWater,
    MatWall,
    MatErase,
    MatCount,
} Material;

static const char* mat_names[MatCount] = {"Sand", "Water", "Wall", "Erase"};

typedef struct {
    uint8_t grid[GH][GW];
    int cx;
    int cy;
    uint8_t mat;
    bool paint;
    bool running;
    uint32_t frame;
    FuriMutex* mutex;
} SandState;

static inline bool in_bounds(int x, int y) {
    return x >= 0 && x < GW && y >= 0 && y < GH;
}

static inline int rnd_dir(void) {
    return (furi_hal_random_get() & 1) ? 1 : -1;
}

static void place(SandState* s) {
    uint8_t v = CellEmpty;
    switch(s->mat) {
    case MatSand:
        v = CellSand;
        break;
    case MatWater:
        v = CellWater;
        break;
    case MatWall:
        v = CellWall;
        break;
    default:
        v = CellEmpty;
        break;
    }
    s->grid[s->cy][s->cx] = v;
}

static void step(SandState* s) {
    // Bottom-up so each particle moves at most one row per tick.
    for(int y = GH - 2; y >= 0; y--) {
        int start = (furi_hal_random_get() & 1) ? 0 : GW - 1;
        int dx = start == 0 ? 1 : -1;
        for(int i = 0; i < GW; i++) {
            int x = start + dx * i;
            uint8_t c = s->grid[y][x];
            if(c == CellSand) {
                uint8_t below = s->grid[y + 1][x];
                if(below == CellEmpty || below == CellWater) {
                    s->grid[y + 1][x] = CellSand;
                    s->grid[y][x] = below;
                    continue;
                }
                int d = rnd_dir();
                for(int k = 0; k < 2; k++) {
                    int nx = x + d;
                    if(in_bounds(nx, y + 1) && s->grid[y][nx] != CellWall) {
                        uint8_t t = s->grid[y + 1][nx];
                        if(t == CellEmpty || t == CellWater) {
                            s->grid[y + 1][nx] = CellSand;
                            s->grid[y][x] = t;
                            break;
                        }
                    }
                    d = -d;
                }
            } else if(c == CellWater) {
                if(s->grid[y + 1][x] == CellEmpty) {
                    s->grid[y + 1][x] = CellWater;
                    s->grid[y][x] = CellEmpty;
                    continue;
                }
                int d = rnd_dir();
                bool moved = false;
                for(int k = 0; k < 2 && !moved; k++) {
                    int nx = x + d;
                    if(in_bounds(nx, y + 1) && s->grid[y + 1][nx] == CellEmpty &&
                       s->grid[y][nx] != CellWall) {
                        s->grid[y + 1][nx] = CellWater;
                        s->grid[y][x] = CellEmpty;
                        moved = true;
                    }
                    d = -d;
                }
                if(moved) continue;
                d = rnd_dir();
                for(int k = 0; k < 2; k++) {
                    int nx = x + d;
                    if(in_bounds(nx, y) && s->grid[y][nx] == CellEmpty) {
                        s->grid[y][nx] = CellWater;
                        s->grid[y][x] = CellEmpty;
                        break;
                    }
                    d = -d;
                }
            }
        }
    }
    // Water on the bottom row spreads sideways too.
    int y = GH - 1;
    for(int x = 0; x < GW; x++) {
        if(s->grid[y][x] == CellWater) {
            int nx = x + rnd_dir();
            if(in_bounds(nx, y) && s->grid[y][nx] == CellEmpty) {
                s->grid[y][nx] = CellWater;
                s->grid[y][x] = CellEmpty;
            }
        }
    }
}

static void draw_cb(Canvas* canvas, void* ctx) {
    SandState* s = ctx;
    furi_mutex_acquire(s->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    for(int y = 0; y < GH; y++) {
        for(int x = 0; x < GW; x++) {
            uint8_t c = s->grid[y][x];
            if(c == CellEmpty) continue;
            int px = x * CELL;
            int py = y * CELL;
            if(c == CellSand || c == CellWall) {
                canvas_draw_box(canvas, px, py, CELL, CELL);
            } else {
                canvas_draw_dot(canvas, px, py);
                canvas_draw_dot(canvas, px + 1, py + 1);
            }
        }
    }

    // Playfield border line + status bar
    canvas_draw_line(canvas, 0, GH * CELL, 127, GH * CELL);
    canvas_set_font(canvas, FontSecondary);
    char buf[40];
    snprintf(
        buf, sizeof(buf), "%s %s", mat_names[s->mat], s->paint ? "[PAINT]" : "");
    canvas_draw_str(canvas, 1, 63, buf);
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "OK:paint Hold:mat");

    // Blinking cursor
    if((s->frame / 4) % 2 == 0 || s->paint) {
        canvas_draw_frame(canvas, s->cx * CELL - 1, s->cy * CELL - 1, CELL + 2, CELL + 2);
    }

    furi_mutex_release(s->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, 0);
}

int32_t sand_sandbox_app(void* p) {
    UNUSED(p);

    SandState* s = malloc(sizeof(SandState));
    memset(s, 0, sizeof(SandState));
    s->cx = GW / 2;
    s->cy = GH / 4;
    s->mat = MatSand;
    s->running = true;
    s->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, s);
    view_port_input_callback_set(vp, input_cb, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(s->running) {
        if(furi_message_queue_get(queue, &ev, 40) == FuriStatusOk) {
            furi_mutex_acquire(s->mutex, FuriWaitForever);
            if(ev.type == InputTypePress || ev.type == InputTypeRepeat) {
                switch(ev.key) {
                case InputKeyUp:
                    if(s->cy > 0) s->cy--;
                    break;
                case InputKeyDown:
                    if(s->cy < GH - 1) s->cy++;
                    break;
                case InputKeyLeft:
                    if(s->cx > 0) s->cx--;
                    break;
                case InputKeyRight:
                    if(s->cx < GW - 1) s->cx++;
                    break;
                case InputKeyBack:
                    s->running = false;
                    break;
                default:
                    break;
                }
                if(s->paint) place(s);
            } else if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
                s->paint = !s->paint;
                if(s->paint) place(s);
            } else if(ev.type == InputTypeLong && ev.key == InputKeyOk) {
                s->mat = (s->mat + 1) % MatCount;
            } else if(ev.type == InputTypeLong && ev.key == InputKeyBack) {
                memset(s->grid, 0, sizeof(s->grid));
            }
            furi_mutex_release(s->mutex);
        }

        furi_mutex_acquire(s->mutex, FuriWaitForever);
        step(s);
        s->frame++;
        furi_mutex_release(s->mutex);
        view_port_update(vp);
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_mutex_free(s->mutex);
    free(s);
    return 0;
}
