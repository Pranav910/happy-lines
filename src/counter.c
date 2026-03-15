#include "counter.h"
#include "platform.h"

#define MAX_IGNORE_FOLDERS 100
#define MAX_IGNORE_LEN     100
#define INITIAL_FILE_CAP   256
#define READ_BUF_SIZE      (128 * 1024)  /* 128KB: fewer syscalls, still stack-safe */

/* Cache line size; pad thread contexts to avoid false sharing */
#define CACHE_LINE_SIZE    64

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
    char         _pad[CACHE_LINE_SIZE];  /* avoid false sharing with next ctx */
};

/* Force mode: thread context for directory-based counting */
struct force_ctx {
    int          start;
    int          end;
    long         total_lines;
    hl_thread_t  handle;
    char       **directories;
    char       (*ignore_folders)[MAX_IGNORE_LEN];
    int          ignore_folders_count;
    char         _pad[CACHE_LINE_SIZE];  /* avoid false sharing with next ctx */
};

/* ─── Line Counting ─── */

static long count_lines(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    setvbuf(fp, NULL, _IOFBF, READ_BUF_SIZE);

    char buf[READ_BUF_SIZE];
    long count = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        const char *p = buf;
        const char *end = buf + n;
        while (p < end) {
            if (*p++ == '\n')
                count++;
        }
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

/* ─── Force mode: recursive directory walk ─── */

static void count_dir_recursive(const char *path,
                                char (*ignore_folders)[MAX_IGNORE_LEN],
                                int ignore_count, long *total) {
    DIR *dr = opendir(path);
    if (!dr) return;

    struct dirent *de;
    char full_path[HL_MAX_PATH];

    while ((de = readdir(dr)) != NULL) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
             (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        snprintf(full_path, sizeof(full_path), "%s%s%s",
                 path, HL_PATH_SEP_STR, de->d_name);

        int is_dir  = (de->d_type == DT_DIR);
        int is_file = (de->d_type == DT_REG);
        if (de->d_type == DT_UNKNOWN || de->d_type == DT_LNK) {
            is_dir  = hl_is_directory(full_path);
            is_file = !is_dir && hl_is_file(full_path);
        }

        if (is_dir) {
            int skip = 0;
            for (int i = 0; i < ignore_count; i++) {
                if (strcmp(de->d_name, ignore_folders[i]) == 0) {
                    skip = 1;
                    break;
                }
            }
            if (!skip)
                count_dir_recursive(full_path, ignore_folders,
                                   ignore_count, total);
        } else if (is_file) {
            *total += count_lines(full_path);
        }
    }
    closedir(dr);
}

static HL_THREAD_FUNC force_worker(void *arg) {
    struct force_ctx *ctx = (struct force_ctx *)arg;
    long local_total = 0;
    for (int i = ctx->start; i <= ctx->end; i++) {
        count_dir_recursive(ctx->directories[i], ctx->ignore_folders,
                           ctx->ignore_folders_count, &local_total);
    }
    ctx->total_lines = local_total;
    HL_THREAD_RETURN;
}

/* ─── Thread Worker (tracked-files mode) ─── */

static HL_THREAD_FUNC count_worker(void *arg) {
    struct thread_ctx *ctx = (struct thread_ctx *)arg;
    long local_total = 0;
    for (int i = ctx->start; i <= ctx->end; i++)
        local_total += count_lines(ctx->files[i]);
    ctx->total_lines = local_total;
    HL_THREAD_RETURN;
}

/* ─── Public API ─── */

int run_loc_count(int threads, int force) {
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

    if (force) {
        /* Force mode: walk all files under current directory (tracked + untracked) */
        DIR *dr = opendir(".");
        if (!dr) {
            fprintf(stderr, "Error: cannot open current directory\n");
            return 1;
        }

        char *directories[512];
        int  num_dirs = 0;
        long root_lines = 0;
        char full_path[HL_MAX_PATH];
        struct dirent *de;

        while ((de = readdir(dr)) != NULL && num_dirs < 512) {
            if (de->d_name[0] == '.' &&
                (de->d_name[1] == '\0' ||
                 (de->d_name[1] == '.' && de->d_name[2] == '\0')))
                continue;

            snprintf(full_path, sizeof(full_path), ".%s%s",
                     HL_PATH_SEP_STR, de->d_name);

            int is_dir  = (de->d_type == DT_DIR);
            int is_file = (de->d_type == DT_REG);
            if (de->d_type == DT_UNKNOWN || de->d_type == DT_LNK) {
                is_dir  = hl_is_directory(full_path);
                is_file = !is_dir && hl_is_file(full_path);
            }

            if (is_dir) {
                int skip = 0;
                for (int i = 0; i < ignore_count; i++) {
                    if (strcmp(de->d_name, ignore_folders[i]) == 0) {
                        skip = 1;
                        break;
                    }
                }
                if (!skip) {
                    directories[num_dirs] = hl_strdup(de->d_name);
                    if (directories[num_dirs])
                        num_dirs++;
                }
            } else if (is_file) {
                root_lines += count_lines(full_path);
            }
        }
        closedir(dr);

        if (threads > MAX_THREADS) threads = MAX_THREADS;
        if (threads > num_dirs)   threads = (num_dirs > 0) ? num_dirs : 1;
        if (threads < 1)         threads = 1;

        long total_lines = root_lines;
        struct force_ctx ctx[MAX_THREADS];
        int nthreads = (num_dirs > 0) ? threads : 0;
        int base = (nthreads > 0) ? num_dirs / nthreads : 0;
        int remainder = (nthreads > 0) ? num_dirs % nthreads : 0;
        int offset = 0;

        for (int i = 0; i < nthreads; i++) {
            int chunk = base + (i < remainder ? 1 : 0);
            ctx[i].start = offset;
            ctx[i].end = offset + chunk - 1;
            offset += chunk;
            ctx[i].total_lines = 0;
            ctx[i].directories = directories;
            ctx[i].ignore_folders = ignore_folders;
            ctx[i].ignore_folders_count = ignore_count;
        }

        double t_start = hl_wall_clock_sec();

        for (int i = 0; i < nthreads; i++)
            hl_thread_create(&ctx[i].handle, force_worker, &ctx[i]);

        for (int i = 0; i < nthreads; i++) {
            hl_thread_join(ctx[i].handle);
            total_lines += ctx[i].total_lines;
        }

        for (int i = 0; i < num_dirs; i++)
            free(directories[i]);

        double elapsed = hl_wall_clock_sec() - t_start;

        printf("Total happy lines count: %ld\n", total_lines);
        printf("Time taken: %f seconds\n", elapsed);
        return 0;
    }

    /* Tracked-files mode: only git-tracked files */
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

    double t_start = hl_wall_clock_sec();

    for (int i = 0; i < threads; i++)
        hl_thread_create(&ctx[i].handle, count_worker, &ctx[i]);

    for (int i = 0; i < threads; i++)
        hl_thread_join(ctx[i].handle);

    long total_lines = 0;
    for (int i = 0; i < threads; i++)
        total_lines += ctx[i].total_lines;

    double elapsed = hl_wall_clock_sec() - t_start;

    printf("Total happy lines count: %ld\n", total_lines);
    printf("Time taken: %f seconds\n", elapsed);

    free_file_list(&fl);
    return 0;
}
