#ifndef HL_ARGPARSER_H
#define HL_ARGPARSER_H

/* ─── Flag type ─── */

enum arg_type { ARG_INT, ARG_BOOL };

/* ─── Flag definition (caller declares these) ─── */

struct arg_def {
    const char   *flag;
    enum arg_type type;
    int           min_val;   /* ARG_INT only: minimum allowed value (inclusive) */
    int           max_val;   /* ARG_INT only: maximum allowed value (inclusive) */
};

/* ─── Conflict rule (pair of flags that cannot coexist) ─── */

struct arg_conflict {
    const char *flag_a;
    const char *flag_b;
};

/* ─── Parsed argument entry ─── */

struct argument {
    char         *flag;
    void         *value;
    enum arg_type type;
};

struct parsed_args {
    struct argument *entries;
    int              count;
};

/**
 * Parse argv against a known set of flag definitions.
 *
 * Validation performed:
 *  - Rejects bare words (arguments not starting with -)
 *  - Rejects single-dash flags (e.g. -force instead of --force)
 *  - Rejects unrecognized flags
 *  - Rejects duplicate flags
 *  - Rejects boolean flags with a value (e.g. --force=1)
 *  - Rejects integer flags without a value (e.g. --file-workers)
 *  - Rejects non-numeric or out-of-range integer values
 *  - Enforces conflict rules (mutually exclusive flag pairs)
 *
 * Returns 0 on success; on failure prints to stderr and returns non-zero.
 */
int parse_arguments(int argc, char *argv[],
                    const struct arg_def *defs, int num_defs,
                    const struct arg_conflict *conflicts, int num_conflicts,
                    struct parsed_args *out);

int  get_int_arg(const struct parsed_args *pa, const char *flag, int fallback);
int  has_flag(const struct parsed_args *pa, const char *flag);
void free_parsed_args(struct parsed_args *pa);

#endif /* HL_ARGPARSER_H */
