#include "platform.h"
#include "argparser.h"
#include "counter.h"
#include "contributor.h"

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --threads=N       Number of threads for parallel LOC "
           "counting (default: 1, max: %d)\n", MAX_THREADS);
    printf("  --force           Count all files (tracked and untracked); "
           "no git required\n");
    printf("  --contributors    Show lines of code per git contributor\n");
    printf("  --help            Show this help message\n");
}

int main(int argc, char *argv[]) {
    struct parsed_args args = parse_arguments(argc, argv);

    if (has_flag(&args, "help")) {
        print_usage(argv[0]);
        free_parsed_args(&args);
        return 0;
    }

    int force = has_flag(&args, "force");
    int show_contributors = has_flag(&args, "contributors");

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

    int threads = get_int_arg(&args, "threads", 1);

    printf("Threads: %d\n", threads);
    if (force)
        printf("Mode: force (all files, tracked + untracked)\n");
    run_loc_count(threads, force);

    if (show_contributors)
        run_contributor_analysis();

    free_parsed_args(&args);
    return 0;
}
