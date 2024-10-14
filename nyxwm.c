#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h> 
#include <X11/Xft/Xft.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include "nyxwm.h"
#include "config.h"
#include "nyxwmblocks.h"

#define DEBUG_LOG(msg, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now)); \
    fprintf(stderr, "[%s] DEBUG: " msg "\n", timestr, ##__VA_ARGS__); \
} while(0)

#define ERROR_LOG(msg, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now)); \
    fprintf(stderr, "[%s] ERROR: " msg ": %s\n", timestr, ##__VA_ARGS__, strerror(errno)); \
} while(0)

static client       *list = {0}, *ws_list[10] = {0}, *cur;
static int          ws = 1, sw, sh, wx, wy, numlock = 0;
static unsigned int ww, wh;
static int          s;
static XButtonEvent mouse;
int num_systray_icons;

Display      *d;
Window       root;
Window bar;
XftFont *xft_font;
XftColor xft_color;
XftDraw *xft_draw;
Window systray;
Window systray_icons[MAX_SYSTRAY_ICONS];
Atom xembed_atom;
Atom manager_atom;
Atom system_tray_opcode_atom;
Atom system_tray_selection_atom;

static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress]      = button_press,
    [ButtonRelease]    = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress]         = key_press,
    [MapRequest]       = map_request,
    [MappingNotify]    = mapping_notify,
    [DestroyNotify]    = notify_destroy,
    [EnterNotify]      = notify_enter,
    [MotionNotify]     = notify_motion
};

#include "config.h"

unsigned long getcolor(const char *col) {
    Colormap m = DefaultColormap(d, s);
    XColor c;
    return (!XAllocNamedColor(d, m, col, &c, &c))?0:c.pixel;
}

void runAutoStart(void) {
    system("cd ~/.nyxwm; ./autostart.sh &");
}

void win_focus(client *c) {
    cur = c;
    XSetInputFocus(d, cur->w, RevertToParent, CurrentTime);
}

void notify_destroy(XEvent *e) {
    win_del(e->xdestroywindow.window);

    if (list) win_focus(list->prev);
}

void notify_enter(XEvent *e) {
    while(XCheckTypedEvent(d, EnterNotify, e));
	while(XCheckTypedWindowEvent(d, mouse.subwindow, MotionNotify, e));

    for win if (c->w == e->xcrossing.window) win_focus(c);
}

void notify_motion(XEvent *e) {
    if (!mouse.subwindow || cur->f) return;

    while(XCheckTypedEvent(d, MotionNotify, e));

    int xd = e->xbutton.x_root - mouse.x_root;
    int yd = e->xbutton.y_root - mouse.y_root;

    XMoveResizeWindow(d, mouse.subwindow,
        wx + (mouse.button == 1 ? xd : 0),
        wy + (mouse.button == 1 ? yd : 0),
        MAX(1, ww + (mouse.button == 3 ? xd : 0)),
        MAX(1, wh + (mouse.button == 3 ? yd : 0)));
}

void key_press(XEvent *e) {
    KeySym keysym = XkbKeycodeToKeysym(d, e->xkey.keycode, 0, 0);

    for (unsigned int i=0; i < sizeof(keys)/sizeof(*keys); ++i)
        if (keys[i].keysym == keysym &&
            mod_clean(keys[i].mod) == mod_clean(e->xkey.state))
            keys[i].function(keys[i].arg);
}

void button_press(XEvent *e) {
    if (!e->xbutton.subwindow) return;

    win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
    XRaiseWindow(d, e->xbutton.subwindow);
    mouse = e->xbutton;
}

void button_release(XEvent *e) {
    mouse.subwindow = 0;
}

void win_add(Window w) {
    client *c;

    if (!(c = (client *) calloc(1, sizeof(client))))
        exit(1);

    c->w = w;

    if (list) {
        list->prev->next = c;
        c->prev          = list->prev;
        list->prev       = c;
        c->next          = list;

    } else {
        list = c;
        list->prev = list->next = list;
    }

    ws_save(ws);
}

void win_del(Window w) {
    client *x = 0;

    for win if (c->w == w) x = c;

    if (!list || !x)  return;
    if (x->prev == x) list = 0;
    if (list == x)    list = x->next;
    if (x->next)      x->next->prev = x->prev;
    if (x->prev)      x->prev->next = x->next;

    free(x);
    ws_save(ws);
}

void win_kill(const Arg arg) {
    if (cur) XKillClient(d, cur->w);
}

void win_center(const Arg arg) {
    if (!cur) return;

    win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);
    XMoveWindow(d, cur->w, (sw - ww) / 2, ((sh - BAR_HEIGHT) - wh) / 2 + BAR_HEIGHT);
}


void win_fs(const Arg arg) {
    if (!cur) return;

    if ((cur->f = cur->f ? 0 : 1)) {
        win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
        int tray_width = num_systray_icons * (TRAY_ICON_SIZE + TRAY_ICON_SPACING) + TRAY_PADDING * 2;
        if (num_systray_icons > 0) {
            tray_width -= TRAY_ICON_SPACING;
        }
        XMoveResizeWindow(d, cur->w, 0, BAR_HEIGHT, sw - tray_width, sh - BAR_HEIGHT);
    } else {
        XMoveResizeWindow(d, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
    }
}

void win_to_ws(const Arg arg) {
    int tmp = ws;

    if (arg.i == tmp) return;

    ws_sel(arg.i);
    win_add(cur->w);
    ws_save(arg.i);

    ws_sel(tmp);
    win_del(cur->w);
    XUnmapWindow(d, cur->w);
    ws_save(tmp);

    if (list) win_focus(list);
}

void win_prev(const Arg arg) {
    if (!cur) return;

    XRaiseWindow(d, cur->prev->w);
    win_focus(cur->prev);
}

void win_next(const Arg arg) {
    if (!cur) return;

    XRaiseWindow(d, cur->next->w);
    win_focus(cur->next);
}

void ws_go(const Arg arg) {
    int tmp = ws;

    if (arg.i == ws) return;

    ws_save(ws);
    ws_sel(arg.i);

    for win XMapWindow(d, c->w);

    ws_sel(tmp);

    for win XUnmapWindow(d, c->w);

    ws_sel(arg.i);

    if (list) win_focus(list); else cur = 0;
}

void configure_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;

    XConfigureWindow(d, ev->window, ev->value_mask, &(XWindowChanges) {
        .x          = ev->x,
        .y          = ev->y,
        .width      = ev->width,
        .height     = ev->height,
        .sibling    = ev->above,
        .stack_mode = ev->detail
    });
}

void map_request(XEvent *e) {
    Window w = e->xmaprequest.window;

    XSelectInput(d, w, StructureNotifyMask|EnterWindowMask);
    win_size(w, &wx, &wy, &ww, &wh);
    win_add(w);
    cur = list->prev;
    XSetWindowBorder(d, w, getcolor(BORDER_COLOR));
    XConfigureWindow(d, w, CWBorderWidth, &(XWindowChanges){.border_width = BORDER_WIDTH});
    
    if (wx + wy == 0) win_center((Arg){0});

    XMapWindow(d, w);
    win_focus(list->prev);
}

void mapping_notify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
        XRefreshKeyboardMapping(ev);
        input_grab(root);
    }
}

void run(const Arg arg) {
    if (fork() == 0) {
        if (d) {
            close(ConnectionNumber(d));
        }
        setsid();
        execvp((char*)arg.com[0], (char**)arg.com);
        fprintf(stderr, "nyxwm: execvp %s", ((char **)arg.com)[0]);
        perror(" failed");
        exit(0);
    }
}

void input_grab(Window root) {
    unsigned int i, j, modifiers[] = {0, LockMask, numlock, numlock|LockMask};
    XModifierKeymap *modmap = XGetModifierMapping(d);
    KeyCode code;

    for (i = 0; i < 8; i++)
        for (int k = 0; k < modmap->max_keypermod; k++)
            if (modmap->modifiermap[i * modmap->max_keypermod + k]
                == XKeysymToKeycode(d, 0xff7f))
                numlock = (1 << i);

    XUngrabKey(d, AnyKey, AnyModifier, root);

    for (i = 0; i < sizeof(keys)/sizeof(*keys); i++)
        if ((code = XKeysymToKeycode(d, keys[i].keysym)))
            for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
                XGrabKey(d, code, keys[i].mod | modifiers[j], root,
                        True, GrabModeAsync, GrabModeAsync);

    for (i = 1; i < 4; i += 2)
        for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
            XGrabButton(d, i, MOD | modifiers[j], root, True,
                ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                GrabModeAsync, GrabModeAsync, 0, 0);

    XFreeModifiermap(modmap);
}

void create_bar() {
    XSetWindowAttributes attr = {
        .override_redirect = True,
        .background_pixel = getcolor(BAR_COLOR)
    };
    
    int bar_width = sw - 2 * TRAY_PADDING; // Adjust if needed

    bar = XCreateWindow(d, root, TRAY_PADDING, 0, bar_width, BAR_HEIGHT, 0,
                        DefaultDepth(d, s), CopyFromParent,
                        DefaultVisual(d, s),
                        CWOverrideRedirect | CWBackPixel, &attr);;
    
    XMapWindow(d, bar);
    
    xft_font = XftFontOpenName(d, s, FONT);  // Use xft_font instead of font
    XftColorAllocName(d, DefaultVisual(d, s), DefaultColormap(d, s), TEXT_COLOR, &xft_color);
    xft_draw = XftDrawCreate(d, bar, DefaultVisual(d, s), DefaultColormap(d, s));
}

void draw_text(const char *text, int x, int y) {
    XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y, (XftChar8*)text, strlen(text));
}

void create_systray() {
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    attr.background_pixel = getcolor(BAR_COLOR);
    systray = XCreateWindow(d, root, sw - TRAY_PADDING, 0, TRAY_PADDING * 2, BAR_HEIGHT, 0,
                            DefaultDepth(d, s), CopyFromParent,
                            DefaultVisual(d, s),
                            CWOverrideRedirect | CWBackPixel, &attr);
    XMapWindow(d, systray);

    xembed_atom = XInternAtom(d, "_XEMBED", False);
    manager_atom = XInternAtom(d, "MANAGER", False);
    system_tray_opcode_atom = XInternAtom(d, "_NET_SYSTEM_TRAY_OPCODE", False);
    system_tray_selection_atom = XInternAtom(d, "_NET_SYSTEM_TRAY_S0", False);

    XSetSelectionOwner(d, system_tray_selection_atom, systray, CurrentTime);
    if (XGetSelectionOwner(d, system_tray_selection_atom) == systray) {
        XClientMessageEvent cm;
        cm.type = ClientMessage;
        cm.window = root;
        cm.message_type = manager_atom;
        cm.format = 32;
        cm.data.l[0] = CurrentTime;
        cm.data.l[1] = system_tray_selection_atom;
        cm.data.l[2] = systray;
        cm.data.l[3] = 0;
        cm.data.l[4] = 0;
        XSendEvent(d, root, False, StructureNotifyMask, (XEvent *)&cm);
        printf("Systray created and manager message sent\n");
    } else {
        printf("Failed to acquire selection ownership for systray\n");
    }
}

void handle_systray_request(XClientMessageEvent *cme) {
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
        Window icon = cme->data.l[2];
        if (num_systray_icons < MAX_SYSTRAY_ICONS) {
            XWindowAttributes wa;
            if (XGetWindowAttributes(d, icon, &wa)) {
                XReparentWindow(d, icon, systray, 
                    TRAY_PADDING + num_systray_icons * (TRAY_ICON_SIZE + TRAY_ICON_SPACING), 
                    (BAR_HEIGHT - TRAY_ICON_SIZE) / 2);
                XResizeWindow(d, icon, TRAY_ICON_SIZE, TRAY_ICON_SIZE);
                XMapRaised(d, icon);
                systray_icons[num_systray_icons++] = icon;

                XEvent ev;
                ev.xclient.type = ClientMessage;
                ev.xclient.window = icon;
                ev.xclient.message_type = xembed_atom;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = CurrentTime;
                ev.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
                ev.xclient.data.l[2] = 0;
                ev.xclient.data.l[3] = systray;
                ev.xclient.data.l[4] = 0;
                XSendEvent(d, icon, False, NoEventMask, &ev);

                XSync(d, False);
                printf("Icon docked: %ld\n", icon);
            } else {
                printf("Failed to get window attributes for icon: %ld\n", icon);
            }
        } else {
            printf("Maximum number of systray icons reached\n");
        }
    } else {
        printf("Received unknown systray request: %ld\n", cme->data.l[1]);
    }
}

void update_systray() {
    static int prev_num_icons = 0;
    static int prev_tray_width = 0;

    int tray_width = num_systray_icons * (TRAY_ICON_SIZE + TRAY_ICON_SPACING) + TRAY_PADDING * 2;
    if (num_systray_icons > 0) {
        tray_width -= TRAY_ICON_SPACING;
    }

    if (num_systray_icons != prev_num_icons || tray_width != prev_tray_width) {
        for (int i = 0; i < num_systray_icons; i++) {
            XMoveResizeWindow(d, systray_icons[i],
                TRAY_PADDING + i * (TRAY_ICON_SIZE + TRAY_ICON_SPACING),
                (BAR_HEIGHT - TRAY_ICON_SIZE) / 2,
                TRAY_ICON_SIZE, TRAY_ICON_SIZE);
        }

        XMoveResizeWindow(d, systray, 
            sw - tray_width, 0,
            tray_width, BAR_HEIGHT);

        prev_num_icons = num_systray_icons;
        prev_tray_width = tray_width;
    }
}

void update_bar() {
    static char prev_status[256] = {0};
    char status[256];

    run_nyxwmblocks(status, sizeof(status));

    if (strcmp(status, prev_status) != 0) {
        XClearWindow(d, bar);

        // Calculate the actual width of the bar
        int bar_width = sw - 2 * TRAY_PADDING; // Adjust if needed

        // Calculate the width of the text
        XGlyphInfo extents;
        XftTextExtentsUtf8(d, xft_font, (XftChar8*)status, strlen(status), &extents);

        // Calculate the starting x position to center the text
        int x = (bar_width - extents.width) / 2;
        int y = BAR_HEIGHT / 2 + xft_font->ascent / 2;

        // Ensure x is not negative
        x = (x < 10) ? 10 : x;

        XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y, (XftChar8*)status, strlen(status));
        XFlush(d);
        strcpy(prev_status, status);
    }
}

int xerror(Display *dpy, XErrorEvent *ee) {
    if (ee->error_code == BadAccess &&
        ee->request_code == X_ChangeWindowAttributes) {
        ERROR_LOG("Another window manager is already running");
        exit(1);
    }

    char error_text[1024];
    XGetErrorText(dpy, ee->error_code, error_text, sizeof(error_text));
    ERROR_LOG("XError: request_code=%d, error_code=%d, error_text=%s", 
              ee->request_code, ee->error_code, error_text);

    return 0;
}

int main(void) {
    DEBUG_LOG("Starting nyxwm");

    if (!(d = XOpenDisplay(NULL))) {
        ERROR_LOG("Cannot open display");
        return 1;
    }
    DEBUG_LOG("Display opened successfully");

    signal(SIGCHLD, SIG_IGN);
    int (*prev_error_handler)(Display *, XErrorEvent *);
    prev_error_handler = XSetErrorHandler(xerror);

    s    = DefaultScreen(d);
    root = RootWindow(d, s);
    sw   = DisplayWidth(d, s);
    sh   = DisplayHeight(d, s);

    DEBUG_LOG("Screen info: s=%d, root=%lu, sw=%d, sh=%d", s, root, sw, sh);

    if (XSelectInput(d, root, SubstructureRedirectMask | SubstructureNotifyMask) == BadWindow) {
        ERROR_LOG("Failed to select input on root window");
        return 1;
    }
    DEBUG_LOG("Input selected on root window");

    XDefineCursor(d, root, XCreateFontCursor(d, 68));
    DEBUG_LOG("Cursor defined");

    input_grab(root);
    DEBUG_LOG("Input grabbed");

    create_bar();
    DEBUG_LOG("Bar created");

    create_systray();
    DEBUG_LOG("Systray created");

    runAutoStart();
    DEBUG_LOG("Autostart run");

    XEvent ev;
    struct timeval tv;
    fd_set fds;
    int xfd = ConnectionNumber(d);

    DEBUG_LOG("Entering main loop");

    while (1) {
        if (XPending(d)) {
            XNextEvent(d, &ev);
            DEBUG_LOG("Received event: type=%d", ev.type);

            if (events[ev.type]) {
                events[ev.type](&ev);
            }

            if (ev.type == ClientMessage && ev.xclient.message_type == system_tray_opcode_atom) {
                handle_systray_request(&ev.xclient);
            } else if (ev.type == DestroyNotify) {
                for (int i = 0; i < num_systray_icons; i++) {
                    if (systray_icons[i] == ev.xdestroywindow.window) {
                        for (int j = i; j < num_systray_icons - 1; j++) {
                            systray_icons[j] = systray_icons[j+1];
                        }
                        num_systray_icons--;
                        break;
                    }
                }
            }
        } else {
            FD_ZERO(&fds);
            FD_SET(xfd, &fds);
            tv.tv_usec = 100000;  // 100ms timeout
            tv.tv_sec = 0;

            int ready = select(xfd + 1, &fds, 0, 0, &tv);
            if (ready == -1) {
                if (errno != EINTR) {
                    ERROR_LOG("select() failed");
                }
            } else if (ready == 0) {
                // Timeout occurred, update bar and systray
                update_bar();
                update_systray();
            }
        }
    }

    // This part will likely never be reached
    DEBUG_LOG("Exiting main loop");
    XSetErrorHandler(prev_error_handler);
    XCloseDisplay(d);
    DEBUG_LOG("Display closed");

    return 0;
}
