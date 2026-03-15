#include "platform.h"
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════
   Thread Implementation
   ═══════════════════════════════════════════════════════════════════ */

#ifdef HL_WINDOWS

int hl_thread_create(hl_thread_t *t, hl_thread_func_t fn, void *arg) {
    *t = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
    return (*t == 0) ? -1 : 0;
}

int hl_thread_join(hl_thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
    return 0;
}

#else

int hl_thread_create(hl_thread_t *t, hl_thread_func_t fn, void *arg) {
    return pthread_create(t, NULL, fn, arg);
}

int hl_thread_join(hl_thread_t t) {
    return pthread_join(t, NULL);
}

#endif

/* ═══════════════════════════════════════════════════════════════════
   Windows Directory Compatibility
   ═══════════════════════════════════════════════════════════════════ */

#ifdef HL_WINDOWS

DIR *opendir(const char *path) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    DIR *dir = (DIR *)calloc(1, sizeof(DIR));
    if (!dir) return NULL;
    dir->handle = FindFirstFileA(pattern, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

struct dirent *readdir(DIR *dir) {
    if (!dir || dir->done) return NULL;
    if (dir->first) {
        dir->first = 0;
    } else if (!FindNextFileA(dir->handle, &dir->find_data)) {
        dir->done = 1;
        return NULL;
    }
    strncpy(dir->entry.d_name, dir->find_data.cFileName, MAX_PATH - 1);
    dir->entry.d_name[MAX_PATH - 1] = '\0';
    dir->entry.d_type =
        (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ? DT_DIR : DT_REG;
    return &dir->entry;
}

int closedir(DIR *dir) {
    if (!dir) return -1;
    if (dir->handle != INVALID_HANDLE_VALUE)
        FindClose(dir->handle);
    free(dir);
    return 0;
}

#endif

/* ═══════════════════════════════════════════════════════════════════
   Utility Functions
   ═══════════════════════════════════════════════════════════════════ */

char *hl_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *dup = (char *)malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

int hl_is_git_repository(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "git rev-parse --is-inside-work-tree 2>%s", HL_DEV_NULL);
    FILE *fp = hl_popen(cmd, "r");
    if (!fp) return 0;
    char buf[16];
    int result = (fgets(buf, sizeof(buf), fp) != NULL &&
                  strncmp(buf, "true", 4) == 0);
    hl_pclose(fp);
    return result;
}

int hl_chdir_to_repo_root(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "git rev-parse --show-toplevel 2>%s", HL_DEV_NULL);
    FILE *fp = hl_popen(cmd, "r");
    if (!fp) return -1;
    char root[HL_MAX_PATH];
    if (!fgets(root, sizeof(root), fp)) {
        hl_pclose(fp);
        return -1;
    }
    hl_pclose(fp);
    root[strcspn(root, "\n\r")] = '\0';
    if (root[0] == '\0') return -1;
    return hl_chdir(root);
}

int hl_is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}

int hl_is_file(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

double hl_wall_clock_sec(void) {
#ifdef HL_WINDOWS
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}
