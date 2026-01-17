#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>

#include "nyxwm.h"
#include "config.h"
#include "nyxwmblocks.h"

static Display *d;
static Window root;
static Window bar;
static XftFont *xft_font;
static XftColor xft_color;
static XftDraw *xft_draw;
static Window systray;
static Window systray_icons[MAX_SYSTRAY_ICONS];
static Atom xembed_atom;
static Atom manager_atom;
static Atom system_tray_opcode_atom;
static Atom system_tray_selection_atom;

static client *list = 0, *ws_list[10] = {0}, *cur;
static int ws = 1, sw, sh, wx, wy, numlock = 0;
static unsigned int ww, wh;
static int s;
static XButtonEvent mouse;
static int num_systray_icons;

static void run_autostart(void);
static void create_bar(void);
static void create_systray(void);
static void update_systray(void);
static void update_bar(void);
static void handle_systray_request(XClientMessageEvent *cme);
static void handle_destroy_notify(XDestroyWindowEvent *ev);

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

unsigned long
getcolor(const char *col)
{
	Colormap m;
	XColor c;

	m = DefaultColormap(d, s);
	return (!XAllocNamedColor(d, m, col, &c, &c)) ? 0 : c.pixel;
}

static void
run_autostart(void)
{
	system("cd ~/.nyxwm; ./autostart.sh &");
}

void
win_focus(client *c)
{
	cur = c;
	XSetInputFocus(d, cur->w, RevertToParent, CurrentTime);
}

void
notify_destroy(XEvent *e)
{
	win_del(e->xdestroywindow.window);

	if (list)
		win_focus(list->prev);
}

void
notify_enter(XEvent *e)
{
	client *t, *c;

	while (XCheckTypedEvent(d, EnterNotify, e))
		;
	while (XCheckTypedWindowEvent(d, mouse.subwindow, MotionNotify, e))
		;

	for win
		if (c->w == e->xcrossing.window)
			win_focus(c);
}

void
notify_motion(XEvent *e)
{
	int xd, yd;

	if (!mouse.subwindow || cur->f)
		return;

	while (XCheckTypedEvent(d, MotionNotify, e))
		;

	xd = e->xbutton.x_root - mouse.x_root;
	yd = e->xbutton.y_root - mouse.y_root;

	XMoveResizeWindow(d, mouse.subwindow,
		wx + (mouse.button == 1 ? xd : 0),
		wy + (mouse.button == 1 ? yd : 0),
		MAX(1, ww + (mouse.button == 3 ? xd : 0)),
		MAX(1, wh + (mouse.button == 3 ? yd : 0)));
}

void
key_press(XEvent *e)
{
	KeySym keysym;
	unsigned int i;

	keysym = XkbKeycodeToKeysym(d, e->xkey.keycode, 0, 0);

	for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
		if (keys[i].keysym == keysym &&
		    mod_clean(keys[i].mod) == mod_clean(e->xkey.state))
			keys[i].function(keys[i].arg);
}

void
button_press(XEvent *e)
{
	if (!e->xbutton.subwindow)
		return;

	win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
	XRaiseWindow(d, e->xbutton.subwindow);
	mouse = e->xbutton;
}

void
button_release(XEvent *e)
{
	mouse.subwindow = 0;
}

void
win_add(Window w)
{
	client *c;

	if (!(c = (client *)calloc(1, sizeof(client))))
		exit(1);

	c->w = w;

	if (list) {
		list->prev->next = c;
		c->prev = list->prev;
		list->prev = c;
		c->next = list;

	} else {
		list = c;
		list->prev = list->next = list;
	}

	ws_save(ws);
}

void
win_del(Window w)
{
	client *t, *c;
	client *x;

	x = 0;
	for win
		if (c->w == w)
			x = c;

	if (!list || !x)
		return;
	if (x->prev == x)
		list = 0;
	if (list == x)
		list = x->next;
	if (x->next)
		x->next->prev = x->prev;
	if (x->prev)
		x->prev->next = x->next;

	free(x);
	ws_save(ws);
}

void
win_kill(const Arg arg)
{
	if (cur)
		XKillClient(d, cur->w);
}

void
win_center(const Arg arg)
{
	if (!cur)
		return;

	win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);
	XMoveWindow(d, cur->w, (sw - ww) / 2,
		((sh - BAR_HEIGHT) - wh) / 2 + BAR_HEIGHT);
}

void
win_fs(const Arg arg)
{
	if (!cur)
		return;

	if ((cur->f = cur->f ? 0 : 1)) {
		win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
		XMoveResizeWindow(d, cur->w, 0, BAR_HEIGHT, sw, sh - BAR_HEIGHT);
		XRaiseWindow(d, cur->w);
	} else {
		XMoveResizeWindow(d, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
	}
	update_systray();
	update_bar();
}

void
win_to_ws(const Arg arg)
{
	int tmp;

	tmp = ws;
	if (arg.i == tmp)
		return;

	ws_sel(arg.i);
	win_add(cur->w);
	ws_save(arg.i);

	ws_sel(tmp);
	win_del(cur->w);
	XUnmapWindow(d, cur->w);
	ws_save(tmp);

	if (list)
		win_focus(list);
}

void
win_prev(const Arg arg)
{
	if (!cur)
		return;

	XRaiseWindow(d, cur->prev->w);
	win_focus(cur->prev);
}

void
win_next(const Arg arg)
{
	if (!cur)
		return;

	XRaiseWindow(d, cur->next->w);
	win_focus(cur->next);
}

void
ws_go(const Arg arg)
{
	client *t, *c;
	int tmp;

	tmp = ws;
	if (arg.i == ws)
		return;

	ws_save(ws);
	ws_sel(arg.i);

	for win
		XMapWindow(d, c->w);

	ws_sel(tmp);

	for win
		XUnmapWindow(d, c->w);

	ws_sel(arg.i);

	if (list)
		win_focus(list);
	else
		cur = 0;
}

void
configure_request(XEvent *e)
{
	XConfigureRequestEvent *ev;

	ev = &e->xconfigurerequest;

	XConfigureWindow(d, ev->window, ev->value_mask, &(XWindowChanges){
		.x = ev->x,
		.y = ev->y,
		.width = ev->width,
		.height = ev->height,
		.sibling = ev->above,
		.stack_mode = ev->detail
	});
}

void
map_request(XEvent *e)
{
	Window w;

	w = e->xmaprequest.window;

	XSelectInput(d, w, StructureNotifyMask | EnterWindowMask);
	win_size(w, &wx, &wy, &ww, &wh);
	win_add(w);
	cur = list->prev;
	XSetWindowBorder(d, w, getcolor(BORDER_COLOR));
	XConfigureWindow(d, w, CWBorderWidth,
		&(XWindowChanges){.border_width = BORDER_WIDTH});

	if (wx + wy == 0)
		win_center((Arg){0});

	XMapWindow(d, w);
	win_focus(list->prev);
}

void
mapping_notify(XEvent *e)
{
	XMappingEvent *ev;

	ev = &e->xmapping;

	if (ev->request == MappingKeyboard || 
		ev->request == MappingModifier) {
		XRefreshKeyboardMapping(ev);
		input_grab(root);
	}
}

void
run(const Arg arg)
{
	if (fork() == 0) {
		if (d)
		close(ConnectionNumber(d));
		setsid();
		execvp((char *)arg.com[0], (char **)arg.com);
		fprintf(stderr, "nyxwm: execvp %s", ((char **)arg.com)[0]);
		perror(" failed");
		exit(0);
	}
}

void
input_grab(Window root)
{
	XModifierKeymap *modmap;
	KeyCode code;
	unsigned int modifiers[] = {0, LockMask, numlock, numlock | LockMask};
	unsigned int i, j;
	int k;

	modmap = XGetModifierMapping(d);

	for (i = 0; i < 8; i++)
		for (k = 0; k < modmap->max_keypermod; k++)
			if (modmap->modifiermap[i * modmap->max_keypermod + k]
			    == XKeysymToKeycode(d, 0xff7f))
				numlock = (1 << i);

	XUngrabKey(d, AnyKey, AnyModifier, root);

	for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
		if ((code = XKeysymToKeycode(d, keys[i].keysym)))
			for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
				XGrabKey(d, code, keys[i].mod | modifiers[j], root,
					True, GrabModeAsync, GrabModeAsync);

	for (i = 1; i < 4; i += 2)
		for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
			XGrabButton(d, i, MOD | modifiers[j], root, True,
				ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
				GrabModeAsync, GrabModeAsync, 0, 0);

	XFreeModifiermap(modmap);
}

static void
create_bar(void)
{
	XSetWindowAttributes attr;
	int bar_width;

	attr.override_redirect = True;
	attr.background_pixel = getcolor(BAR_COLOR);

	bar_width = sw - 2 * TRAY_PADDING;

	bar = XCreateWindow(d, root, TRAY_PADDING, 0, bar_width,
		BAR_HEIGHT, 0, DefaultDepth(d, s), CopyFromParent,
		DefaultVisual(d, s), CWOverrideRedirect | CWBackPixel, &attr);

	XMapWindow(d, bar);

	xft_font = XftFontOpenName(d, s, FONT);
	XftColorAllocName(d, DefaultVisual(d, s), DefaultColormap(d, s),
		TEXT_COLOR, &xft_color);
	xft_draw = XftDrawCreate(d, bar, DefaultVisual(d, s),
		DefaultColormap(d, s));
}

void
draw_text(const char *text, int x, int y)
{
	XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y,
		(XftChar8 *)text, strlen(text));
}

static void
create_systray(void)
{
	XSetWindowAttributes attr;

	attr.override_redirect = True;
	attr.background_pixel = getcolor(BAR_COLOR);
	systray = XCreateWindow(d, root, sw - TRAY_PADDING, 0,
		TRAY_PADDING * 2, BAR_HEIGHT, 0, DefaultDepth(d, s),
		CopyFromParent, DefaultVisual(d, s),
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
	}
}

static void
update_systray(void)
{
	int tray_width;
	int bar_width;
	int i;

	tray_width = num_systray_icons * (TRAY_ICON_SIZE + TRAY_ICON_SPACING)
		+ TRAY_PADDING * 2;
	if (num_systray_icons > 0) {
		tray_width -= TRAY_ICON_SPACING;

		XMapWindow(d, systray);
		XMoveResizeWindow(d, systray, sw - tray_width, 0,
			tray_width, BAR_HEIGHT);

		for (i = 0; i < num_systray_icons; i++) {
			XMoveResizeWindow(d, systray_icons[i],
				TRAY_PADDING + i * (TRAY_ICON_SIZE + TRAY_ICON_SPACING),
				(BAR_HEIGHT - TRAY_ICON_SIZE) / 2,
				TRAY_ICON_SIZE, TRAY_ICON_SIZE);
			XMapWindow(d, systray_icons[i]);
		}
	} else {
		XUnmapWindow(d, systray);
		tray_width = 0;
	}

	bar_width = sw - tray_width;
	XMoveResizeWindow(d, bar, 0, 0, bar_width, BAR_HEIGHT);

	XRaiseWindow(d, bar);
	if (num_systray_icons > 0)
		XRaiseWindow(d, systray);

	update_bar();
}

static void
handle_systray_request(XClientMessageEvent *cme)
{
	Window icon;
	XWindowAttributes wa;
	XEvent ev;

	if (cme->data.l[1] != SYSTEM_TRAY_REQUEST_DOCK)
		return;

	icon = cme->data.l[2];
	if (num_systray_icons >= MAX_SYSTRAY_ICONS)
		return;

	if (XGetWindowAttributes(d, icon, &wa)) {
		systray_icons[num_systray_icons++] = icon;
		XReparentWindow(d, icon, systray, 0, 0);
		XMapRaised(d, icon);

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
		update_systray();
	}
}

static void
update_bar(void)
{
	char status[256];
	int bar_width;
	int x, y;
	XGlyphInfo extents;

	run_nyxwmblocks(status, sizeof(status));

	XClearWindow(d, bar);

	bar_width = sw - (num_systray_icons > 0
		? (num_systray_icons * (TRAY_ICON_SIZE + TRAY_ICON_SPACING)
		+ TRAY_PADDING * 2 - TRAY_ICON_SPACING)
		: 0);

	XftTextExtentsUtf8(d, xft_font, (XftChar8 *)status,
		strlen(status), &extents);

	x = (bar_width - extents.width) / 2;
	y = BAR_HEIGHT / 2 + xft_font->ascent / 2;

	x = (x < 10) ? 10 : x;

	XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y,
		(XftChar8 *)status, strlen(status));
	XFlush(d);
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	char error_text[1024];

	if (ee->error_code == BadAccess &&
	    ee->request_code == X_ChangeWindowAttributes)
		exit(1);

	XGetErrorText(dpy, ee->error_code, error_text, sizeof(error_text));
	return 0;
}

static void
handle_destroy_notify(XDestroyWindowEvent *ev)
{
	int i, j;

	for (i = 0; i < num_systray_icons; i++) {
		if (systray_icons[i] == ev->window) {
			for (j = i; j < num_systray_icons - 1; j++)
				systray_icons[j] = systray_icons[j + 1];
			num_systray_icons--;
			update_systray();
			break;
		}
	}
}

int
main(void)
{
	int (*prev_error_handler)(Display *, XErrorEvent *);
	XEvent ev;
	struct timeval tv, last_update, now;
	fd_set fds;
	int xfd;
	int ready;

	if (!(d = XOpenDisplay(NULL)))
		return 1;

	signal(SIGCHLD, SIG_IGN);
	prev_error_handler = XSetErrorHandler(xerror);

	s = DefaultScreen(d);
	root = RootWindow(d, s);
	sw = DisplayWidth(d, s);
	sh = DisplayHeight(d, s);

	if (XSelectInput(d, root, SubstructureRedirectMask
	    | SubstructureNotifyMask) == BadWindow)
		return 1;

	XDefineCursor(d, root, XCreateFontCursor(d, 68));
	input_grab(root);
	create_bar();
	create_systray();
	run_autostart();

	gettimeofday(&last_update, NULL);
	xfd = ConnectionNumber(d);

	while (1) {
		while (XPending(d)) {
			XNextEvent(d, &ev);

			if (events[ev.type])
				events[ev.type](&ev);

			if (ev.type == ClientMessage
			    && ev.xclient.message_type == system_tray_opcode_atom) {
				handle_systray_request(&ev.xclient);
			} else if (ev.type == DestroyNotify) {
				handle_destroy_notify(&ev.xdestroywindow);
				update_systray();
			} else if (ev.type == MapNotify
			    || ev.type == UnmapNotify) {
				update_systray();
			}
		}

		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		tv.tv_usec = 100000;
		tv.tv_sec = 0;

		ready = select(xfd + 1, &fds, 0, 0, &tv);
		if (ready < 0) {
			if (errno != EINTR)
				printf("select() failed");
		} else if (ready == 0) {
			gettimeofday(&now, NULL);
			if ((now.tv_sec - last_update.tv_sec) * 1000000
			    + (now.tv_usec - last_update.tv_usec) >= 1000000) {
				update_bar();
				update_systray();
				last_update = now;
			}
		}
	}

	XSetErrorHandler(prev_error_handler);
	XCloseDisplay(d);

	return 0;
}
