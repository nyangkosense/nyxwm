// nyxwmblocks.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include "nyxwm.h"
#include "nyxwmblocks.h"
#include "blocks.h"

#define LENGTH(X) (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH 100
#define MIN(a, b) ((a < b) ? a : b)
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

void getcmd(const Block *block, char *output);
void getsigcmds(unsigned int signal);
void getstatus(char *str);
void setupsignals(void);
void update_status(void);
void statusloop(void);
void termhandler(int signum);
void pstdout(void);
void sighandler(int signum);

static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[STATUSLENGTH];
static int statusContinue = 1;

extern Display *d;
extern Window root;
extern Window bar;
extern XftDraw *xft_draw;
extern XftColor xft_color;
extern XftFont *xft_font;

void getcmd(const Block *block, char *output) {
    char tempstatus[CMDLENGTH] = {0};
    strcpy(tempstatus, block->icon);
    FILE *cmdf = popen(block->command, "r");
    if (!cmdf)
        return;
    int i = strlen(block->icon);
    if (fgets(tempstatus + i, CMDLENGTH - i, cmdf) != NULL) {
        i = strlen(tempstatus);
        if (i > 0 && tempstatus[i-1] == '\n') {
            tempstatus[--i] = '\0';
        }
        if (delim[0] != '\0' && i + delimLen < CMDLENGTH) {
            strncpy(tempstatus + i, delim, CMDLENGTH - i - 1);
            tempstatus[CMDLENGTH - 1] = '\0';
        }
    }
    strcpy(output, tempstatus);
    pclose(cmdf);
}

void getcmds(int time)
{
    const Block* current;
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        current = blocks + i;
        if ((current->interval != 0 && time % current->interval == 0) || time == -1)
            getcmd(current, statusbar[i]);
    }
}

void getsigcmds(unsigned int signal)
{
    const Block *current;
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        current = blocks + i;
        if (current->signal == signal)
            getcmd(current, statusbar[i]);
    }
}

void setupsignals() {
    for (unsigned int i = 0; i < LENGTH(blocks); i++) {
        if (blocks[i].signal > 0)
            signal(SIGRTMIN+blocks[i].signal, sighandler);
    }
}

void getstatus(char *str)
{
    str[0] = '\0';
    for (unsigned int i = 0; i < LENGTH(blocks); i++)
        strcat(str, statusbar[i]);
    str[strlen(str)-strlen(delim)] = '\0';
}

void update_status() {
    if (!d || !bar || !xft_draw || !xft_font)
        return;

    getstatus(statusstr);

    XClearWindow(d, bar);

    int bar_width = DisplayWidth(d, DefaultScreen(d));
    XGlyphInfo extents;
    XftTextExtentsUtf8(d, xft_font, (XftChar8*)statusstr, strlen(statusstr), &extents);

    int x = (bar_width - extents.width) / 2;
    int y = BAR_HEIGHT / 2 + xft_font->ascent / 2;

    // Ensure x is not negative and has a minimum padding
    x = (x < 10) ? 10 : x;

    XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y, (XftChar8*)statusstr, strlen(statusstr));
    XFlush(d);
}

void statusloop()
{
    setupsignals();
    int i = 0;
    getcmds(-1);
    while (statusContinue) {
        getcmds(i++);
        update_status();
        sleep(1.0);
    }
}

void sighandler(int signum)
{
    getsigcmds(signum-SIGRTMIN);
    update_status();
}

void termhandler(int signum) {
    statusContinue = 0;
    (void)signum;  // To avoid unused parameter warning
}

void pstdout()
{
    getstatus(statusstr);
    printf("%s\n", statusstr);
    fflush(stdout);
}

// This function should be called from nyxwm's main loop
void run_nyxwmblocks(char *status, size_t size) {
    static int time = 0;

    getcmds(time);
    getstatus(status);

    time++;
    if (time == 60) time = 0;  // Reset every minute to avoid potential overflow
}
