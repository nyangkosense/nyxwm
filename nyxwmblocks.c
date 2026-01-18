/* nyxwmblocks.c */
#include <stdio.h>
#include <string.h>

#include "blocks.h"
#include "nyxwmblocks.h"

#define LENGTH(X)  (sizeof(X) / sizeof(X[0]))
#define CMDLENGTH  100

static char statusbar[LENGTH(blocks)][CMDLENGTH] = { 0 };

static void getcmd(const Block * block, char *output);
static void getcmds(int time);
static void getstatus(char *str);

static void
getcmd(const Block *block, char *output)
{
  char tempstatus[CMDLENGTH];
  FILE *cmdf;
  int i;

  memset(tempstatus, 0, sizeof(tempstatus));
  strcpy(tempstatus, block->icon);
  cmdf = popen(block->command, "r");
  if (!cmdf)
    return;

  i = strlen(block->icon);
  if (fgets(tempstatus + i, CMDLENGTH - i, cmdf) != NULL) {
    i = strlen(tempstatus);
    if (i > 0 && tempstatus[i - 1] == '\n')
      tempstatus[--i] = '\0';
    if (delim[0] != '\0' && i + delimLen < CMDLENGTH) {
      strncpy(tempstatus + i, delim, CMDLENGTH - i - 1);
      tempstatus[CMDLENGTH - 1] = '\0';
    }
  }
  strcpy(output, tempstatus);
  pclose(cmdf);
}

static void
getcmds(int time)
{
  const Block *current;
  unsigned int i;

  for (i = 0; i < LENGTH(blocks); i++) {
    current = blocks + i;
    if ((current->interval != 0 && time % current->interval == 0)
	|| time == -1)
      getcmd(current, statusbar[i]);
  }
}

static void
getstatus(char *str)
{
  unsigned int i;

  str[0] = '\0';
  for (i = 0; i < LENGTH(blocks); i++)
    strcat(str, statusbar[i]);
  str[strlen(str) - strlen(delim)] = '\0';
}

void
run_nyxwmblocks(char *status, size_t size)
{
  static int time = 0;

  (void) size;
  getcmds(time);
  getstatus(status);

  time++;
  if (time == 60)
    time = 0;
}
