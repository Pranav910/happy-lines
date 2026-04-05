#include "counter.h"
#include "platform.h"
#include "hash_map.h"

#include <stdlib.h>
#include <string.h>

#define MAX_IGNORE_FOLDERS 100
#define MAX_IGNORE_LEN     100
#define INITIAL_FILE_CAP   256
#define MAX_EXTENSIONS     22
#define READ_BUF_SIZE      (128 * 1024)

/* Pad thread contexts to avoid false sharing between adjacent array slots */
#define CACHE_LINE_SIZE 64

#define EXT_TABLE_RULE "-------------------------------"

/* ─── Internal Types ─── */

typedef struct {
    const char *ext;
    long        lines;
} extension_stat_t;

static extension_stat_t s_ext_totals[MAX_EXTENSIONS] = {
    { "c", 0 },
    { "h", 0 },
    { "cpp", 0 },
    { "hpp", 0 },
    { "java", 0 },
    { "py", 0 },
    { "js", 0 },
    { "ts", 0 },
    {"json", 0 },
    {"jsonc", 0 },
    {"yml", 0 },
    {"yaml", 0 },
    {"md", 0 },
    {"sql", 0 },
    {"sh", 0 },
    {"gitignore", 0 },
    {"dockerignore", 0 },
    {"nvmrc", 0 },
    {"toml", 0 },
    {"lock", 0 },
    {"tsx", 0 },
    {"jsx", 0 },
};

struct file_list {
    char **paths;
    int    count;
    int    capacity;
};

struct file_worker_ctx {
    int            start;
    int            end;
    long           total_lines;
    hl_thread_t    handle;
    char         **files;
    int            by_extension;
    hl_hash_map_t *extension_map;
    char           _pad[CACHE_LINE_SIZE];
};

struct dir_walker_ctx {
    int              start;
    int              end;
    hl_thread_t      handle;
    char           **directories;
    char           (*ignore_folders)[MAX_IGNORE_LEN];
    int              ignore_folders_count;
    struct file_list collector;
    char             _pad[CACHE_LINE_SIZE];
};

/* ─── Helpers ─── */

static const char *extension_from_path(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

static int cmp_extension_lines_desc(const void *a, const void *b) {
    long la = ((const extension_stat_t *)a)->lines;
    long lb = ((const extension_stat_t *)b)->lines;
    return (lb > la) - (lb < la);
}

static void print_extension_breakdown(void) {
    extension_stat_t rows[MAX_EXTENSIONS];
    int              n = 0;

    for (int i = 0; i < MAX_EXTENSIONS; i++) {
        if (s_ext_totals[i].lines > 0) rows[n++] = s_ext_totals[i];
    }
    if (n == 0) return;

    qsort(rows, (size_t)n, sizeof(rows[0]), cmp_extension_lines_desc);

    printf("\n%s\n", EXT_TABLE_RULE);
    printf(" %-16s %12s\n", "Extension", "Lines");
    printf("%s\n", EXT_TABLE_RULE);

    long sum = 0;
    for (int i = 0; i < n; i++) {
        printf(" %-16s %12ld\n", rows[i].ext, rows[i].lines);
        sum += rows[i].lines;
    }

    printf("%s\n", EXT_TABLE_RULE);
    printf(" %-16s %12ld\n", "TOTAL", sum);
    printf("%s\n", EXT_TABLE_RULE);
}

static void print_time_taken(double elapsed_sec) {
    if (elapsed_sec > 60.0)
        printf("Time taken: %f minutes\n", elapsed_sec / 60.0);
    else
        printf("Time taken: %f seconds\n", elapsed_sec);
}

static int file_list_push(struct file_list *fl, const char *path) {
    if (fl->count >= fl->capacity) {
        int    new_cap = (fl->capacity == 0) ? INITIAL_FILE_CAP : fl->capacity * 2;
        char **resized = (char **)realloc(fl->paths, sizeof(char *) * (size_t)new_cap);
        if (!resized) return -1;
        fl->paths    = resized;
        fl->capacity = new_cap;
    }
    fl->paths[fl->count] = hl_strdup(path);
    if (!fl->paths[fl->count]) return -1;
    fl->count++;
    return 0;
}

static int clamp_workers(int requested, int max_allowed, int work_items) {
    if (requested > max_allowed) requested = max_allowed;
    if (work_items > 0 && requested > work_items) requested = work_items;
    if (requested < 1) requested = 1;
    return requested;
}

/* ─── Line Counting ─── */

static long count_lines(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    setvbuf(fp, NULL, _IOFBF, READ_BUF_SIZE);

    char   buf[READ_BUF_SIZE];
    long   count = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        const char *p   = buf;
        const char *end = buf + n;
        while (p < end) {
            if (*p++ == '\n') count++;
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
    if (!fl.paths) {
        hl_pclose(fp);
        return fl;
    }

    char line[HL_MAX_PATH];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n\r")] = '\0';
        if (line[0] == '\0') continue;

        if (fl.count >= fl.capacity) {
            int     new_cap = fl.capacity * 2;
            char **resized  = (char **)realloc(fl.paths, sizeof(char *) * (size_t)new_cap);
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
    for (int i = 0; i < fl->count; i++) free(fl->paths[i]);
    free(fl->paths);
    fl->paths    = NULL;
    fl->count    = 0;
    fl->capacity = 0;
}

/* ─── Directory Walker (force mode, phase 1) ─── */

static void collect_files_recursive(const char *path,
                                    char (*ignore_folders)[MAX_IGNORE_LEN],
                                    int ignore_count,
                                    struct file_list *collector) {
    DIR *dr = opendir(path);
    if (!dr) return;

    struct dirent *de;
    char           full_path[HL_MAX_PATH];

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
                collect_files_recursive(full_path, ignore_folders,
                                        ignore_count, collector);
        } else if (is_file) {
            file_list_push(collector, full_path);
        }
    }
    closedir(dr);
}

static HL_THREAD_FUNC dir_walk_worker(void *arg) {
    struct dir_walker_ctx *ctx = (struct dir_walker_ctx *)arg;
    for (int i = ctx->start; i <= ctx->end; i++) {
        collect_files_recursive(ctx->directories[i], ctx->ignore_folders,
                                ctx->ignore_folders_count, &ctx->collector);
    }
    HL_THREAD_RETURN;
}

/* ─── File Worker (shared by both modes) ─── */

static HL_THREAD_FUNC file_worker(void *arg) {
    struct file_worker_ctx *ctx = (struct file_worker_ctx *)arg;
    long local_total = 0;

    for (int i = ctx->start; i <= ctx->end; i++) {
        long lines = count_lines(ctx->files[i]);
        local_total += lines;

        if (!ctx->by_extension) continue;

        const char *ext = extension_from_path(ctx->files[i]);
        if (ext[0] == '\0') continue;

        int prev = hl_hash_map_get(ctx->extension_map, ext);
        if (prev < 0) prev = 0;
        hl_hash_map_put(ctx->extension_map, ext, prev + (int)lines);
    }
    ctx->total_lines = local_total;
    HL_THREAD_RETURN;
}

/* ─── Dispatch file workers over a file list ─── */

static long dispatch_file_workers(struct file_list *fl, int file_workers,
                                  int by_extension) {
    int n_fw = clamp_workers(file_workers, MAX_FILE_WORKERS, fl->count);

    struct file_worker_ctx ctx[MAX_FILE_WORKERS];
    int base      = fl->count / n_fw;
    int remainder = fl->count % n_fw;
    int offset    = 0;

    for (int i = 0; i < n_fw; i++) {
        int chunk = base + (i < remainder ? 1 : 0);
        ctx[i].start         = offset;
        ctx[i].end           = offset + chunk - 1;
        offset              += chunk;
        ctx[i].total_lines   = 0;
        ctx[i].files         = fl->paths;
        ctx[i].by_extension  = by_extension;
        ctx[i].extension_map = NULL;
        if (by_extension) {
            ctx[i].extension_map = hl_hash_map_create(MAX_EXTENSIONS);
            if (!ctx[i].extension_map) {
                fprintf(stderr, "Error: cannot allocate extension map\n");
                for (int j = 0; j < i; j++)
                    hl_hash_map_destroy(ctx[j].extension_map);
                return -1;
            }
        }
    }

    for (int i = 0; i < n_fw; i++)
        hl_thread_create(&ctx[i].handle, file_worker, &ctx[i]);

    for (int i = 0; i < n_fw; i++)
        hl_thread_join(ctx[i].handle);

    long total_lines = 0;
    for (int i = 0; i < n_fw; i++)
        total_lines += ctx[i].total_lines;

    if (by_extension) {
        for (int j = 0; j < MAX_EXTENSIONS; j++)
            s_ext_totals[j].lines = 0;

        for (int i = 0; i < n_fw; i++) {
            for (int j = 0; j < MAX_EXTENSIONS; j++) {
                int part = hl_hash_map_get(ctx[i].extension_map,
                                           s_ext_totals[j].ext);
                if (part >= 0) s_ext_totals[j].lines += part;
            }
            hl_hash_map_destroy(ctx[i].extension_map);
            ctx[i].extension_map = NULL;
        }

        print_extension_breakdown();
    }

    return total_lines;
}

/* ─── Public API ─── */

int run_loc_count(int dir_workers, int file_workers, int force, int by_extension) {
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

    printf("\nProcessing files...\n");

    if (force) {
        /* ── Phase 0: enumerate top-level entries ── */
        DIR *dr = opendir(".");
        if (!dr) {
            fprintf(stderr, "Error: cannot open current directory\n");
            return 1;
        }

        char *directories[512];
        int   num_dirs = 0;
        struct file_list root_files = { NULL, 0, 0 };
        char  full_path[HL_MAX_PATH];
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
                    if (directories[num_dirs]) num_dirs++;
                }
            } else if (is_file) {
                file_list_push(&root_files, full_path);
            }
        }
        closedir(dr);

        double t_start = hl_wall_clock_sec();

        /* ── Phase 1: directory walkers discover files ── */
        int n_dw = (num_dirs > 0)
                       ? clamp_workers(dir_workers, MAX_DIR_WORKERS, num_dirs)
                       : 0;
        struct dir_walker_ctx dctx[MAX_DIR_WORKERS];

        if (n_dw > 0) {
            int base      = num_dirs / n_dw;
            int remainder = num_dirs % n_dw;
            int offset    = 0;

            for (int i = 0; i < n_dw; i++) {
                int chunk = base + (i < remainder ? 1 : 0);
                dctx[i].start                = offset;
                dctx[i].end                  = offset + chunk - 1;
                offset                      += chunk;
                dctx[i].directories          = directories;
                dctx[i].ignore_folders       = ignore_folders;
                dctx[i].ignore_folders_count = ignore_count;
                dctx[i].collector.paths      = NULL;
                dctx[i].collector.count      = 0;
                dctx[i].collector.capacity   = 0;
            }

            for (int i = 0; i < n_dw; i++)
                hl_thread_create(&dctx[i].handle, dir_walk_worker, &dctx[i]);

            for (int i = 0; i < n_dw; i++)
                hl_thread_join(dctx[i].handle);
        }

        /* ── Merge discovered files into one list ── */
        int total_files = root_files.count;
        for (int i = 0; i < n_dw; i++)
            total_files += dctx[i].collector.count;

        struct file_list merged = { NULL, 0, 0 };
        if (total_files > 0) {
            merged.paths = (char **)malloc(sizeof(char *) * (size_t)total_files);
            if (!merged.paths) {
                fprintf(stderr, "Error: out of memory\n");
                for (int i = 0; i < n_dw; i++)
                    free_file_list(&dctx[i].collector);
                free_file_list(&root_files);
                for (int i = 0; i < num_dirs; i++) free(directories[i]);
                return 1;
            }
            merged.capacity = total_files;

            for (int i = 0; i < root_files.count; i++)
                merged.paths[merged.count++] = root_files.paths[i];

            for (int i = 0; i < n_dw; i++) {
                for (int j = 0; j < dctx[i].collector.count; j++)
                    merged.paths[merged.count++] = dctx[i].collector.paths[j];
                free(dctx[i].collector.paths);
                dctx[i].collector.paths = NULL;
                dctx[i].collector.count = 0;
            }
        }
        /* String ownership transferred to merged; only free the array shells */
        free(root_files.paths);
        root_files.paths = NULL;
        for (int i = 0; i < num_dirs; i++) free(directories[i]);

        /* ── Phase 2: file workers count lines ── */
        long total_lines = 0;
        if (merged.count > 0) {
            total_lines = dispatch_file_workers(&merged, file_workers, 0);
            if (total_lines < 0) {
                free_file_list(&merged);
                return 1;
            }
        }

        double elapsed = hl_wall_clock_sec() - t_start;

        printf("\nTotal happy lines count: %ld\n", total_lines);
        print_time_taken(elapsed);

        free_file_list(&merged);
        return 0;
    }

    /* ── Git-tracked mode ── */

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

    double t_start = hl_wall_clock_sec();

    long total_lines = dispatch_file_workers(&fl, file_workers, by_extension);
    if (total_lines < 0) {
        free_file_list(&fl);
        return 1;
    }

    double elapsed = hl_wall_clock_sec() - t_start;

    printf("\nTotal happy lines count: %ld\n", total_lines);
    print_time_taken(elapsed);

    free_file_list(&fl);
    return 0;
}
