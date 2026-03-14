#include "counter.h"
#include "platform.h"
#include <time.h>

#define MAX_IGNORE_FOLDERS 100
#define MAX_IGNORE_LEN     100
#define INITIAL_FILE_CAP   256

/* ─── Internal Types ─── */

struct file_list {
    char **paths;
    int    count;
    int    capacity;
};

struct thread_ctx {
    int          id;
    int          start;
    int          end;
    long         total_lines;
    hl_thread_t  handle;
    char       **files;
};

/* ─── Line Counting ─── */

static int count_lines(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
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

/* ─── Git Tracked File Collection ─── */

static struct file_list get_tracked_files(void) {
    struct file_list fl = { NULL, 0, 0 };

    FILE *fp = hl_popen("git ls-files", "r");
    if (!fp) return fl;

    fl.paths    = (char **)malloc(sizeof(char *) * INITIAL_FILE_CAP);
    fl.capacity = INITIAL_FILE_CAP;
    if (!fl.paths) { hl_pclose(fp); return fl; }

    char line[HL_MAX_PATH];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n\r")] = '\0';
        if (line[0] == '\0') continue;

        if (fl.count >= fl.capacity) {
            int new_cap    = fl.capacity * 2;
            char **resized = (char **)realloc(fl.paths,
                                              sizeof(char *) * new_cap);
            if (!resized) break;
            fl.paths    = resized;
            fl.capacity = new_cap;
        }

        fl.paths[fl.count] = hl_strdup(line);
        if (!fl.paths[fl.count]) break;
        fl.count++;
    }

    hl_pclose(fp);
    return fl;
}

static void filter_ignored(struct file_list *fl,
                           char (*folders)[MAX_IGNORE_LEN],
                           int folder_count) {
    if (folder_count == 0) return;

    int write_idx = 0;
    for (int i = 0; i < fl->count; i++) {
        int ignored = 0;
        for (int j = 0; j < folder_count; j++) {
            size_t len = strlen(folders[j]);
            if (strncmp(fl->paths[i], folders[j], len) == 0 &&
                (fl->paths[i][len] == '/' || fl->paths[i][len] == '\\' ||
                 fl->paths[i][len] == '\0')) {
                ignored = 1;
                break;
            }
        }
        if (ignored) {
            free(fl->paths[i]);
        } else {
            fl->paths[write_idx++] = fl->paths[i];
        }
    }
    fl->count = write_idx;
}

static void free_file_list(struct file_list *fl) {
    for (int i = 0; i < fl->count; i++)
        free(fl->paths[i]);
    free(fl->paths);
    fl->paths    = NULL;
    fl->count    = 0;
    fl->capacity = 0;
}

/* ─── Thread Worker ─── */

static HL_THREAD_FUNC count_worker(void *arg) {
    struct thread_ctx *ctx = (struct thread_ctx *)arg;
    for (int i = ctx->start; i <= ctx->end; i++)
        ctx->total_lines += count_lines(ctx->files[i]);
    HL_THREAD_RETURN;
}

/* ─── Public API ─── */

int run_loc_count(int threads) {
    char ignore_folders[MAX_IGNORE_FOLDERS][MAX_IGNORE_LEN];
    int  ignore_count = 0;
    char input[MAX_IGNORE_LEN];

    printf("Enter the folders to ignore (exit to stop):\n");
    while (ignore_count < MAX_IGNORE_FOLDERS) {
        if (scanf("%99s", input) != 1) break;
        if (strcmp(input, "exit") == 0) break;
        strncpy(ignore_folders[ignore_count], input, MAX_IGNORE_LEN - 1);
        ignore_folders[ignore_count][MAX_IGNORE_LEN - 1] = '\0';
        ignore_count++;
    }

    struct file_list fl = get_tracked_files();
    if (fl.count == 0) {
        printf("No tracked files found.\n");
        free_file_list(&fl);
        return 0;
    }

    filter_ignored(&fl, ignore_folders, ignore_count);

    if (fl.count == 0) {
        printf("All tracked files were excluded by ignore rules.\n");
        free_file_list(&fl);
        return 0;
    }

    if (threads > MAX_THREADS) threads = MAX_THREADS;
    if (threads > fl.count)    threads = fl.count;
    if (threads < 1)           threads = 1;

    struct thread_ctx ctx[MAX_THREADS];
    int base      = fl.count / threads;
    int remainder = fl.count % threads;
    int offset    = 0;

    for (int i = 0; i < threads; i++) {
        int chunk          = base + (i < remainder ? 1 : 0);
        ctx[i].id          = i + 1;
        ctx[i].start       = offset;
        ctx[i].end         = offset + chunk - 1;
        offset            += chunk;
        ctx[i].total_lines = 0;
        ctx[i].files       = fl.paths;
    }

    clock_t t_start = clock();

    for (int i = 0; i < threads; i++)
        hl_thread_create(&ctx[i].handle, count_worker, &ctx[i]);

    for (int i = 0; i < threads; i++)
        hl_thread_join(ctx[i].handle);

    long total_lines = 0;
    for (int i = 0; i < threads; i++)
        total_lines += ctx[i].total_lines;

    clock_t t_end   = clock();
    double  elapsed = (double)(t_end - t_start) / CLOCKS_PER_SEC;

    printf("Total happy lines count: %ld\n", total_lines);
    printf("Time taken: %f seconds\n", elapsed);

    free_file_list(&fl);
    return 0;
}
