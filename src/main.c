#include "platform.h"
#include "argparser.h"
#include "counter.h"
#include "contributor.h"

static const struct arg_def known_flags[] = {
    { "directory-workers", ARG_INT,  1, MAX_DIR_WORKERS  },
    { "file-workers",     ARG_INT,  1, MAX_FILE_WORKERS },
    { "force",            ARG_BOOL, 0, 0 },
    { "contributors",     ARG_BOOL, 0, 0 },
    { "by-extension",     ARG_BOOL, 0, 0 },
    { "help",             ARG_BOOL, 0, 0 },
};
#define NUM_FLAGS ((int)(sizeof(known_flags) / sizeof(known_flags[0])))

static const struct arg_conflict conflicts[] = {
    { "force", "by-extension" },
};
#define NUM_CONFLICTS ((int)(sizeof(conflicts) / sizeof(conflicts[0])))

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --directory-workers=N  Threads for parallel directory traversal "
           "(default: CPU count, max: %d)\n", MAX_DIR_WORKERS);
    printf("  --file-workers=N      Threads for parallel file processing "
           "(default: CPU count, max: %d)\n", MAX_FILE_WORKERS);
    printf("  --force               Count all files (tracked and untracked); "
           "no git required\n");
    printf("  --contributors        Show lines of code per git contributor\n");
    printf("  --by-extension        Show lines of code per file extension "
           "(git-tracked only; not with --force)\n");
    printf("  --help                Show this help message\n");
}

int main(int argc, char *argv[]) {
    struct parsed_args args;

    if (parse_arguments(argc, argv,
                        known_flags, NUM_FLAGS,
                        conflicts, NUM_CONFLICTS,
                        &args) != 0) {
        return 1;
    }

    if (has_flag(&args, "help")) {
        print_usage(argv[0]);
        free_parsed_args(&args);
        return 0;
    }

    int force = has_flag(&args, "force");
    int show_contributors = has_flag(&args, "contributors");
    int by_extension = has_flag(&args, "by-extension");

    if (!force) {
        if (!hl_is_git_repository()) {
            fprintf(stderr,
                    "Error: not inside a git repository.\n"
                    "happy-lines must be run from within a git repository.\n"
                    "Use --force to count all files in the current directory.\n");
            free_parsed_args(&args);
            return 1;
        }
        if (hl_chdir_to_repo_root() != 0) {
            fprintf(stderr, "Error: cannot change to repository root\n");
            free_parsed_args(&args);
            return 1;
        }
    }

    int cpus         = hl_cpu_count();
    int dir_workers  = get_int_arg(&args, "directory-workers", cpus);
    int file_workers = get_int_arg(&args, "file-workers", cpus);

    if (force)
        printf("Directory workers: %d\nFile workers: %d\n"
               "Mode: force (all files, tracked + untracked)\n",
               dir_workers, file_workers);
    else
        printf("File workers: %d\n", file_workers);

    run_loc_count(dir_workers, file_workers, force, by_extension);

    if (show_contributors)
        run_contributor_analysis();

    free_parsed_args(&args);
    return 0;
}
