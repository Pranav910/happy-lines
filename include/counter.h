#ifndef HL_COUNTER_H
#define HL_COUNTER_H

#define MAX_THREADS 10

/**
 * Run LOC counting. When force is 0, only git-tracked files are counted.
 * When force is 1, all files under the current directory are counted
 * (tracked and untracked), excluding folders specified at the prompt.
 */
int run_loc_count(int threads, int force, int by_extension);

#endif /* HL_COUNTER_H */
