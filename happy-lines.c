#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define MAIN_HEADER_TITLE_Y 0
#define MAIN_HEADER_TITLE_X 0
#define HIDE_CURSOR "\x1b[?25l"
#define RESTORE_TERMINAL "\x1b[3;1H\x1b[2K\n"
#define CLEAR_SCREEN "\x1b[2J\x1b[H"
#define DRAW_AT_FORMAT "\x1b[%d;%dH"
#define SHOW_CURSOR "\x1b[?25h"

int is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  return S_ISDIR(st.st_mode);
}

int is_file(const char *path) {
  struct stat st;

  if (stat(path, &st) != 0) {
    return 0;
  }

  return S_ISREG(st.st_mode);
}

int count_lines(const char *path) {
  FILE *fp = fopen(path, "r");
  int happy_lines_count = 0;

  if (fp == NULL) {
    printf("Error opening file %s\n", "example_dir/example.txt");
    return 1;
  }

  for (char c = getc(fp); c != EOF; c = getc(fp)) {
    if (c == '\n')
      happy_lines_count++;
  }

  ++happy_lines_count;
  fclose(fp);

  return happy_lines_count;
}

void count_happy_lines(const char *path, const char *cwd,
                       int *total_happy_lines_count, DIR *dr,
                       char ignore_folders[100][100],
                       int ignore_folders_count) {

  for (int i = 0; i < ignore_folders_count; i++) {
    if (strcmp(cwd, ignore_folders[i]) == 0) {
      return;
    }
  }

  if (!dr) {
    return;
  }

  struct dirent *de;
  char full_path[1000];

  while ((de = readdir(dr)) != NULL) {
    if (*de->d_name == '.' || strcmp(de->d_name, "..") == 0) {
      continue;
    }
    snprintf(full_path, sizeof(full_path), "%s/%s", path, de->d_name);
    if (is_directory(full_path)) {
      count_happy_lines(full_path, de->d_name, total_happy_lines_count,
                        opendir(full_path), ignore_folders,
                        ignore_folders_count);
    } else if (is_file(full_path)) {
      *total_happy_lines_count += count_lines(full_path);
    }
  }
}

void clear() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
  system("clear");
#elif defined(_WIN32) || defined(_WIN64)
  system("cls");
#endif
}

static struct termios orig_termios;

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

void restore_terminal() {
  write(STDOUT_FILENO, RESTORE_TERMINAL, sizeof(RESTORE_TERMINAL) - 1);
}

int draw_menu() {
  char ignore_folders[100][100];
  int ignore_folders_count = 0;
  char ignore_folder[100];

  printf("Enter the folders to ignore (exit to stop): \n");

  while (1) {
    scanf("%s", ignore_folder);
    if (strcmp(ignore_folder, "exit") == 0) {
      break;
    }
    strcpy(ignore_folders[ignore_folders_count], ignore_folder);
    ignore_folders_count++;
  }

  struct dirent *de;
  char path[100] = ".";
  DIR *dr = opendir(path);
  int total_happy_lines_count = 0;
  int total_directories = 0;
  enable_raw_mode();
  int x = 4;

  while ((de = readdir(dr)) != NULL) {
    if (is_directory(de->d_name) && strcmp(de->d_name, "..") != 0) {
      total_directories++;
    }
  }

  char **arr = (char **)malloc(sizeof(char *) * total_directories);
  int i = 0;

  rewinddir(dr);

  while ((de = readdir(dr)) != NULL) {
    if (is_directory(de->d_name) && strcmp(de->d_name, "..") != 0) {
      arr[i] = (char *)malloc(sizeof(char) * strlen(de->d_name));
      strcpy(arr[i], de->d_name);
      i++;
    }
  }

  int y = 2;

  while (1) {
    clear_screen();
    draw_at(MAIN_HEADER_TITLE_Y, MAIN_HEADER_TITLE_X, "Select a directory:");

    for (int j = 0; j < total_directories; j++) {
      draw_at(j + 2, x, arr[j]);
    }

    draw_at(y, 1, ">");
    write(STDOUT_FILENO, "\x1b[H", 3);

    int key = read_key();
    if (key == 'q')
      return 0;
    if (key == KEY_ENTER)
      break;

    if (key == KEY_DOWN && y < total_directories + 1)
      y++;
    if (key == KEY_UP && y > 2)
      y--;
  }

  clear_screen();

  draw_at(1, 0, "Selected directory: %s", arr[y - 2]);

  dr = opendir(arr[y - 2]);

  count_happy_lines(arr[y - 2], arr[y - 2], &total_happy_lines_count, dr,
                    ignore_folders, ignore_folders_count);

  draw_at(2, 0, "Total happy lines count: %d", total_happy_lines_count);

  closedir(dr);
  return 0;
}

int main() {
  draw_menu();
  restore_terminal();
  return 0;
}