#ifndef HL_PLATFORM_H
#define HL_PLATFORM_H

/* ═══════════════════════════════════════════════════════════════════
   Platform Detection
   ═══════════════════════════════════════════════════════════════════ */

#if defined(_WIN32) || defined(_WIN64)
  #define HL_WINDOWS
#endif

#if defined(__APPLE__) && defined(__MACH__)
  #define HL_MACOS
#endif

#if defined(__linux__)
  #define HL_LINUX
#endif

/* ═══════════════════════════════════════════════════════════════════
   Platform Headers
   ═══════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HL_WINDOWS
  #include <windows.h>
  #include <process.h>
  #include <direct.h>
  #include <io.h>

  #ifdef _MSC_VER
    #pragma warning(disable : 4996)
  #endif
#else
  #include <dirent.h>
  #include <pthread.h>
  #include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════
   Compatibility Macros
   ═══════════════════════════════════════════════════════════════════ */

#ifdef HL_WINDOWS
  #define hl_popen        _popen
  #define hl_pclose       _pclose
  #define hl_getcwd       _getcwd
  #define hl_chdir        _chdir
  #define HL_PATH_SEP     '\\'
  #define HL_PATH_SEP_STR "\\"
  #define HL_DEV_NULL     "nul"
  #define HL_MAX_PATH     MAX_PATH

  #ifndef S_ISDIR
    #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
  #endif
  #ifndef S_ISREG
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
  #endif
#else
  #define hl_popen        popen
  #define hl_pclose       pclose
  #define hl_getcwd       getcwd
  #define hl_chdir        chdir
  #define HL_PATH_SEP     '/'
  #define HL_PATH_SEP_STR "/"
  #define HL_DEV_NULL     "/dev/null"
  #define HL_MAX_PATH     4096
#endif

/* ═══════════════════════════════════════════════════════════════════
   Thread Abstraction
   ═══════════════════════════════════════════════════════════════════ */

#ifdef HL_WINDOWS
  typedef HANDLE hl_thread_t;
  typedef unsigned (__stdcall *hl_thread_func_t)(void *);

  #define HL_THREAD_FUNC   unsigned __stdcall
  #define HL_THREAD_RETURN return 0
#else
  typedef pthread_t hl_thread_t;
  typedef void *(*hl_thread_func_t)(void *);

  #define HL_THREAD_FUNC   void *
  #define HL_THREAD_RETURN return NULL
#endif

int hl_thread_create(hl_thread_t *t, hl_thread_func_t fn, void *arg);
int hl_thread_join(hl_thread_t t);

/* ═══════════════════════════════════════════════════════════════════
   Windows Directory Compatibility Layer
   ═══════════════════════════════════════════════════════════════════ */

#ifdef HL_WINDOWS
  #ifndef DT_DIR
    #define DT_DIR     4
    #define DT_REG     8
    #define DT_UNKNOWN 0
    #define DT_LNK    10
  #endif

  struct dirent {
      char          d_name[MAX_PATH];
      unsigned char d_type;
  };

  typedef struct {
      HANDLE            handle;
      WIN32_FIND_DATAA  find_data;
      struct dirent     entry;
      int               first;
      int               done;
  } DIR;

  DIR            *opendir(const char *path);
  struct dirent  *readdir(DIR *dir);
  int             closedir(DIR *dir);
#endif

/* ═══════════════════════════════════════════════════════════════════
   Utility Functions
   ═══════════════════════════════════════════════════════════════════ */

char *hl_strdup(const char *s);
int   hl_is_git_repository(void);
int   hl_chdir_to_repo_root(void);
int   hl_is_directory(const char *path);
int   hl_is_file(const char *path);

/** Wall-clock time in seconds (monotonic). Use for measuring elapsed time. */
double hl_wall_clock_sec(void);

/** Number of logical CPUs available (falls back to 1 on error). */
int hl_cpu_count(void);

#endif /* HL_PLATFORM_H */
