#include <dirent.h>
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

void count_happy_lines(const char *path, int *total_happy_lines_count, DIR *dr) {
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
      count_happy_lines(full_path, total_happy_lines_count, opendir(full_path));
      strcpy(full_path, path);
    } else if (is_file(full_path)) {
      *total_happy_lines_count +=
          count_lines(full_path);
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
  write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
  perror(msg);
  exit(1);
}

void disable_raw_mode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  write(STDOUT_FILENO, "\x1b[?25h", 6); // show cursor
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

  write(STDOUT_FILENO, "\x1b[?25l", 6); // hide cursor
}

void clear_screen() { write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7); }

void draw_at(int y, int x, char *ch) {
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH%s", y, x, ch);
  write(STDOUT_FILENO, buf, len);
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

int main() {
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
      break;
    if (key == KEY_ENTER)
      break;

    if (key == KEY_DOWN && y < total_directories + 1)
      y++;
    if (key == KEY_UP && y > 2)
      y--;
  }
  clear_screen();
  printf("Selected directory: %s\n", arr[y - 2]);
  if (strcmp(path, ".") == 0) {
    strcat(strcat(path, "/"), arr[y - 2]);
  }
  dr = opendir(path);
  count_happy_lines(path, &total_happy_lines_count, dr);

  printf("Total happy lines count: %d\n", total_happy_lines_count);

  closedir(dr);
  return 0;
}