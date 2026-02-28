#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define HIDE_CURSOR "\x1b[?25l"
#define RESTORE_TERMINAL "\x1b[3;1H\x1b[2K\n"
#define CLEAR_SCREEN "\x1b[2J\x1b[H"
#define DRAW_AT_FORMAT "\x1b[%d;%dH"
#define SHOW_CURSOR "\x1b[?25h"

static struct termios orig_termios;

void draw_at(int y, int x, const char *fmt, ...) {
  char buf[1000];
  int offset = sprintf(buf, DRAW_AT_FORMAT, y, x);
  va_list args;

  va_start(args, fmt);
  offset += vsnprintf(buf + offset, sizeof(buf) - offset, fmt, args);
  va_end(args);
  write(STDOUT_FILENO, buf, offset);
}

enum { KEY_UP = 1000, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER };

int read_key() {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return -1;

  if (c == '\x1b') {
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        return KEY_UP;
      case 'B':
        return KEY_DOWN;
      case 'C':
        return KEY_RIGHT;
      case 'D':
        return KEY_LEFT;
      }
    }
    return '\x1b';
  }
  if (c == '\r' || c == '\n')
    return KEY_ENTER;

  return c;
}

void clear() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
  system("clear");
#elif defined(_WIN32) || defined(_WIN64)
  system("cls");
#endif
}

void die(const char *msg) {
  write(STDOUT_FILENO, CLEAR_SCREEN, sizeof(CLEAR_SCREEN) - 1);
  perror(msg);
  exit(1);
}

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  write(STDOUT_FILENO, SHOW_CURSOR, sizeof(SHOW_CURSOR) - 1);
}

void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcgetattr");

  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_oflag &= ~(OPOST);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");

  write(STDOUT_FILENO, HIDE_CURSOR, sizeof(HIDE_CURSOR) - 1);
}

void clear_screen() {
  write(STDOUT_FILENO, CLEAR_SCREEN, sizeof(CLEAR_SCREEN) - 1);
}



void restore_terminal() {
  write(STDOUT_FILENO, RESTORE_TERMINAL, sizeof(RESTORE_TERMINAL) - 1);
}
