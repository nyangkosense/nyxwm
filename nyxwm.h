#ifndef NYXWM_H
#define NYXWM_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xatom.h>

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define XEMBED_REQUEST_FOCUS        3
#define XEMBED_FOCUS_IN             4
#define XEMBED_FOCUS_OUT            5
#define XEMBED_FOCUS_NEXT           6
#define XEMBED_FOCUS_PREV           7

#define XEMBED_MODALITY_ON          10
#define XEMBED_MODALITY_OFF         11
#define XEMBED_REGISTER_ACCELERATOR 12
#define XEMBED_UNREGISTER_ACCELERATOR 13
#define XEMBED_ACTIVATE_ACCELERATOR 14


#ifndef MAX_SYSTRAY_ICONS
#define MAX_SYSTRAY_ICONS 20
#endif

#ifndef BAR_HEIGHT
#define BAR_HEIGHT 23
#endif

#define win        (client *t=0, *c=list; c && t!=list->prev; t=c, c=c->next)
#define ws_save(W) ws_list[W] = list
#define ws_sel(W)  list = ws_list[ws = W]
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

#define win_size(W, gx, gy, gw, gh) \
    XGetGeometry(d, W, &(Window){0}, gx, gy, gw, gh, \
                 &(unsigned int){0}, &(unsigned int){0})

// Taken from DWM. Many thanks. https://git.suckless.org/dwm
#define mod_clean(mask) (mask & ~(numlock|LockMask) & \
        (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

typedef struct {
    char* icon;
    char* command;
    unsigned int interval;
    unsigned int signal;
} Block;

typedef struct {
    const char** com;
    const int i;
    const Window w;
} Arg;

struct key {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
};

typedef struct client {
    struct client *next, *prev;
    int f, wx, wy;
    unsigned int ww, wh;
    Window w;
} client;

unsigned long getcolor(const char *col);
void button_press(XEvent *e);
void button_release(XEvent *e);
void configure_request(XEvent *e);
void input_grab(Window root);
void key_press(XEvent *e);
void map_request(XEvent *e);
void mapping_notify(XEvent *e);
void notify_destroy(XEvent *e);
void notify_enter(XEvent *e);
void notify_motion(XEvent *e);
void run(const Arg arg);
void runAutoStart(void);
void win_add(Window w);
void win_center(const Arg arg);
void win_del(Window w);
void win_fs(const Arg arg);
void win_focus(client *c);
void win_kill(const Arg arg);
void win_prev(const Arg arg);
void win_next(const Arg arg);
void win_to_ws(const Arg arg);
void ws_go(const Arg arg);
void create_bar();
void update_bar();
void draw_text(const char *text, int x, int y);
void create_systray();
void handle_systray_request(XClientMessageEvent *cme);
void update_systray();
void run_nyxwmblocks();
void handle_destroy_notify(XDestroyWindowEvent *ev);
int xerror(Display *dpy, XErrorEvent *ee);

extern Window bar;
extern XftFont *xft_font;
extern XftColor xft_color;
extern XftDraw *xft_draw;
extern Window systray;
extern Window systray_icons[MAX_SYSTRAY_ICONS];
extern int num_systray_icons;
extern Atom xembed_atom;
extern Atom manager_atom;
extern Atom system_tray_opcode_atom;
extern Atom system_tray_selection_atom;

#endif
