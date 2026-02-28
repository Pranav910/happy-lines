#include "argparser.c"
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define MAIN_HEADER_TITLE_Y 0
#define MAIN_HEADER_TITLE_X 0
#define DIRECTORY_MODE 0
#define FILE_MODE 1
#define MAX_THREADS 10

struct thread {
  int thread_id;
  int start;
  int end;
  int total_happy_lines_count;
  pthread_t thread;
  char **directories;
  char (*ignore_folders)[100];
  int ignore_folders_count;
  int mode;
};

int str_cmp(const char *str1, const char *str2) {
  if (strlen(str1) != strlen(str2)) {
    return 0;
  }
  return strcmp(str1, str2) == 0;
}

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
  if (!fp)
    return 0;
  char buf[65536];
  int count = 0;
  size_t bytes;
  while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
    for (size_t i = 0; i < bytes; i++)
      if (buf[i] == '\n')
        count++;
  }
  fclose(fp);
  return count + 1;
}

void count_happy_lines(const char *path, const char *cwd,
                       int *total_happy_lines_count, DIR *dr,
                       char (*ignore_folders)[100], int ignore_folders_count,
                       int mode) {

  for (int i = 0; i < ignore_folders_count; i++) {
    if (strcmp(cwd, ignore_folders[i]) == 0) {
      if (dr)
        closedir(dr);
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

    int entry_is_dir = (de->d_type == DT_DIR);
    int entry_is_file = (de->d_type == DT_REG);
    if (de->d_type == DT_UNKNOWN || de->d_type == DT_LNK) {
      entry_is_dir = is_directory(full_path);
      entry_is_file = !entry_is_dir && is_file(full_path);
    }

    if (mode == DIRECTORY_MODE && entry_is_dir) {
      count_happy_lines(full_path, de->d_name, total_happy_lines_count,
                        opendir(full_path), ignore_folders,
                        ignore_folders_count, mode);
    } else if (entry_is_file) {
      *total_happy_lines_count += count_lines(full_path);
    }
  }
  closedir(dr);
}

void *run(void *arg) {
  struct thread *thread = (struct thread *)arg;
  for (int i = thread->start; i <= thread->end; ++i) {
    count_happy_lines(thread->directories[i], thread->directories[i],
                      &thread->total_happy_lines_count,
                      opendir(thread->directories[i]), thread->ignore_folders,
                      thread->ignore_folders_count, thread->mode);
  }
  return NULL;
}

int draw_menu(int threads) {
  struct thread thread[MAX_THREADS];

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
  int x = 4;

  while ((de = readdir(dr)) != NULL) {
    if (is_directory(de->d_name) && strcmp(de->d_name, "..") != 0 &&
        strcmp(de->d_name, ".") != 0) {
      total_directories++;
    }
  }

  char **directories = (char **)malloc(sizeof(char *) * total_directories);
  int i = 0;

  rewinddir(dr);

  while ((de = readdir(dr)) != NULL) {
    if (is_directory(de->d_name) && strcmp(de->d_name, "..") != 0 &&
        strcmp(de->d_name, ".") != 0) {
      directories[i] = (char *)malloc(sizeof(char) * (strlen(de->d_name) + 1));
      strcpy(directories[i], de->d_name);
      i++;
    }
  }

  closedir(dr);

  if (threads > total_directories) {
    threads = total_directories;
  }

  int base = total_directories / threads;
  int remainder = total_directories % threads;
  int offset = 0;

  for (int i = 0; i < threads; ++i) {
    int chunk = base + (i < remainder ? 1 : 0);
    thread[i].thread_id = i + 1;
    thread[i].start = offset;
    thread[i].end = offset + chunk - 1;
    offset += chunk;
    thread[i].total_happy_lines_count = 0;
    thread[i].directories = directories;
    thread[i].ignore_folders = ignore_folders;
    thread[i].ignore_folders_count = ignore_folders_count;
    thread[i].mode = DIRECTORY_MODE;
  }

  clock_t start, end;
  double cpu_time_used;

  start = clock();

  for (int i = 0; i < threads; ++i) {
    pthread_create(&thread[i].thread, NULL, run, (void *)&thread[i]);
  }

  for (int i = 0; i < threads; ++i) {
    pthread_join(thread[i].thread, NULL);
  }

  for (int i = 0; i < threads; ++i) {
    total_happy_lines_count += thread[i].total_happy_lines_count;
  }

  int total_happy_lines_count_file = 0;

  count_happy_lines(".", ".", &total_happy_lines_count_file, opendir("."),
                    ignore_folders, ignore_folders_count, FILE_MODE);

  end = clock();
  cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

  printf("Total happy lines count: %d\n",
         total_happy_lines_count_file + total_happy_lines_count);
  printf("Time taken: %f seconds\n", cpu_time_used);

  return 0;
}

void list_directories(const char *path) {
  DIR *dr = opendir(path);
  struct dirent *de;
  while ((de = readdir(dr)) != NULL) {
    if (is_directory(de->d_name) && strcmp(de->d_name, "..") != 0) {
      printf("%s\n", de->d_name);
    }
  }
}

int main(int argc, char *argv[]) {
  struct arguments *arguments = parse_arguments(argc, argv);
  int threads;
  if (arguments != NULL) {
    for (int i = 0; i < argc - 1; i++) {
      threads = *(int *)arguments[i].value;
    }
  }
  printf("Threads: %d\n", threads);

  draw_menu(threads);
  return 0;
}