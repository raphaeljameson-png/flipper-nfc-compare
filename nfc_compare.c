#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include "nfc_compare_io.h"

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* input_queue;
    char status[64];
    NfcProfile profile_a;
    NfcProfile profile_b;
    NfcProfile profile_phys;
} App;

static void render_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "NFC Compare");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, "Gauche: A (fichier)");
    canvas_draw_str(canvas, 2, 38, "Bas: B (fichier)");
    canvas_draw_str(canvas, 2, 50, "Droite: Scan carte");
    canvas_draw_str(canvas, 2, 62, "OK: A vs carte  Haut: A vs B");
    canvas_draw_str(canvas, 2, 76, app->status);
}

static void input_callback(InputEvent* input, void* ctx) {
    App* app = ctx;
    if(input->type == InputTypePress || input->type == InputTypeShort) {
        furi_message_queue_put(app->input_queue, input, 0);
    }
}

static void set_status(App* app, const char* msg) {
    furi_check(snprintf(app->status, sizeof(app->status), "%s", msg) > 0);
}

int32_t nfc_compare_app(void* p) {
    UNUSED(p);
    App app = {0};
    set_status(&app, "Pret");

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.vp = view_port_alloc();
    view_port_draw_callback_set(app.vp, render_callback, &app);
    view_port_input_callback_set(app.vp, input_callback, &app);

    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.vp, GuiLayerFullscreen);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Nfc* nfc = furi_record_open(RECORD_NFC);

    bool running = true;
    InputEvent ev;

    while(running) {
        if(furi_message_queue_get(app.input_queue, &ev, 100) == FuriStatusOk) {
            switch(ev.key) {
                case InputKeyBack:
                    running = false;
                    break;
                case InputKeyLeft: {
                    if(load_nfc_profile_from_sd(storage, &app.profile_a)) {
                        set_status(&app, "A: fichier charge");
                    } else {
                        set_status(&app, "Echec chargement A");
                    }
                } break;
                case InputKeyDown: {
                    if(load_nfc_profile_from_sd(storage, &app.profile_b)) {
                        set_status(&app, "B: fichier charge");
                    } else {
                        set_status(&app, "Echec chargement B");
                    }
                } break;
                case InputKeyRight: {
                    if(scan_nfc_physical(nfc, &app.profile_phys)) {
                        set_status(&app, "Carte scannee");
                    } else {
                        set_status(&app, "Echec scan");
                    }
                } break;
                case InputKeyOk: {
                    int res = compare_profiles(&app.profile_a, &app.profile_phys);
                    if(res == 0) set_status(&app, "Match A vs Carte");
                    else set_status(&app, "Diff A vs Carte");
                } break;
                case InputKeyUp: {
                    int res = compare_profiles(&app.profile_a, &app.profile_b);
                    if(res == 0) set_status(&app, "Match A vs B");
                    else set_status(&app, "Diff A vs B");
                } break;
                default: break;
            }
            view_port_update(app.vp);
        }
    }

    furi_record_close(RECORD_NFC);
    furi_record_close(RECORD_STORAGE);
    gui_remove_view_port(app.gui, app.vp);
    furi_record_close(RECORD_GUI);
    view_port_free(app.vp);
    furi_message_queue_free(app.input_queue);
    free_nfc_profile(&app.profile_a);
    free_nfc_profile(&app.profile_b);
    free_nfc_profile(&app.profile_phys);
    return 0;
}
