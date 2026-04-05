#ifndef HL_COUNTER_H
#define HL_COUNTER_H

#define MAX_DIR_WORKERS  32
#define MAX_FILE_WORKERS 32

/**
 * Run LOC counting with separate directory and file worker pools.
 *
 * dir_workers:   threads for parallel directory traversal (force mode only)
 * file_workers:  threads for parallel file line counting
 * force:         1 = count all files; 0 = git-tracked only
 * by_extension:  1 = show per-extension breakdown (git-tracked only)
 */
int run_loc_count(int dir_workers, int file_workers, int force, int by_extension);

#endif /* HL_COUNTER_H */
