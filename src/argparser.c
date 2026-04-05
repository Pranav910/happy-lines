#include "argparser.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Helpers ─── */

static int find_equals(const char *arg) {
    for (int i = 2; arg[i] != '\0'; i++) {
        if (arg[i] == '=')
            return i;
    }
    return -1;
}

static const struct arg_def *lookup_def(const char *flag,
                                        const struct arg_def *defs,
                                        int num_defs) {
    for (int i = 0; i < num_defs; i++) {
        if (strcmp(defs[i].flag, flag) == 0)
            return &defs[i];
    }
    return NULL;
}

static int is_valid_integer(const char *s, int *out) {
    if (s[0] == '\0') return 0;

    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;

    *out = (int)v;
    return 1;
}

/* ─── Public API ─── */

int parse_arguments(int argc, char *argv[],
                    const struct arg_def *defs, int num_defs,
                    const struct arg_conflict *conflicts, int num_conflicts,
                    struct parsed_args *out) {
    out->entries = NULL;
    out->count   = 0;

    if (argc < 2)
        return 0;

    out->entries = (struct argument *)calloc((size_t)(argc - 1),
                                            sizeof(struct argument));
    if (!out->entries) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* ── Must start with a dash ── */
        if (arg[0] != '-') {
            fprintf(stderr, "Error: unexpected argument '%s' "
                    "(flags must start with --)\n", arg);
            free_parsed_args(out);
            return 1;
        }

        /* ── Single-dash detection ── */
        if (arg[0] == '-' && arg[1] != '-') {
            fprintf(stderr,
                    "Error: invalid flag '%s' (use -- prefix, e.g. --%s)\n",
                    arg, arg + 1);
            free_parsed_args(out);
            return 1;
        }

        /* ── Empty after -- ── */
        if (arg[2] == '\0') {
            fprintf(stderr, "Error: empty flag '--'\n");
            free_parsed_args(out);
            return 1;
        }

        /* ── Extract flag name ── */
        int  eq       = find_equals(arg);
        int  flag_len = (eq > 0) ? (eq - 2) : ((int)strlen(arg) - 2);
        char flag_buf[128];

        if (flag_len <= 0 || flag_len >= (int)sizeof(flag_buf)) {
            fprintf(stderr, "Error: malformed flag '%s'\n", arg);
            free_parsed_args(out);
            return 1;
        }
        memcpy(flag_buf, arg + 2, (size_t)flag_len);
        flag_buf[flag_len] = '\0';

        /* ── Look up in definitions ── */
        const struct arg_def *def = lookup_def(flag_buf, defs, num_defs);
        if (!def) {
            fprintf(stderr, "Error: unknown flag '--%s'\n", flag_buf);
            free_parsed_args(out);
            return 1;
        }

        /* ── Reject duplicates ── */
        for (int j = 0; j < out->count; j++) {
            if (strcmp(out->entries[j].flag, flag_buf) == 0) {
                fprintf(stderr, "Error: duplicate flag '--%s'\n", flag_buf);
                free_parsed_args(out);
                return 1;
            }
        }

        int idx = out->count;

        if (def->type == ARG_BOOL) {
            /* Bool flags must not carry a value */
            if (eq > 0) {
                fprintf(stderr,
                        "Error: flag '--%s' is a boolean and does not accept "
                        "a value (remove '=%s')\n",
                        flag_buf, arg + eq + 1);
                free_parsed_args(out);
                return 1;
            }

            out->entries[idx].flag = (char *)malloc((size_t)flag_len + 1);
            if (!out->entries[idx].flag) goto oom;
            memcpy(out->entries[idx].flag, flag_buf, (size_t)flag_len + 1);

            int *val = (int *)malloc(sizeof(int));
            if (!val) { free(out->entries[idx].flag); goto oom; }
            *val = 1;
            out->entries[idx].value = val;
            out->entries[idx].type  = ARG_BOOL;

        } else {
            /* Integer flags require =N */
            if (eq < 0) {
                fprintf(stderr,
                        "Error: flag '--%s' requires a value "
                        "(e.g. --%s=%d)\n",
                        flag_buf, flag_buf, def->min_val);
                free_parsed_args(out);
                return 1;
            }

            const char *val_str = arg + eq + 1;
            int         ival;

            if (!is_valid_integer(val_str, &ival)) {
                fprintf(stderr,
                        "Error: '--%s=%s' is not a valid integer\n",
                        flag_buf, val_str);
                free_parsed_args(out);
                return 1;
            }

            if (ival < def->min_val || ival > def->max_val) {
                fprintf(stderr,
                        "Error: '--%s=%d' is out of range "
                        "(allowed: %d..%d)\n",
                        flag_buf, ival, def->min_val, def->max_val);
                free_parsed_args(out);
                return 1;
            }

            out->entries[idx].flag = (char *)malloc((size_t)flag_len + 1);
            if (!out->entries[idx].flag) goto oom;
            memcpy(out->entries[idx].flag, flag_buf, (size_t)flag_len + 1);

            int *pval = (int *)malloc(sizeof(int));
            if (!pval) { free(out->entries[idx].flag); goto oom; }
            *pval = ival;
            out->entries[idx].value = pval;
            out->entries[idx].type  = ARG_INT;
        }

        out->count++;
    }

    /* ── Enforce conflict rules ── */
    for (int c = 0; c < num_conflicts; c++) {
        int has_a = has_flag(out, conflicts[c].flag_a);
        int has_b = has_flag(out, conflicts[c].flag_b);
        if (has_a && has_b) {
            fprintf(stderr,
                    "Error: --%s and --%s cannot be used together\n",
                    conflicts[c].flag_a, conflicts[c].flag_b);
            free_parsed_args(out);
            return 1;
        }
    }

    return 0;

oom:
    fprintf(stderr, "Error: out of memory\n");
    free_parsed_args(out);
    return 1;
}

int get_int_arg(const struct parsed_args *pa, const char *flag, int fallback) {
    for (int i = 0; i < pa->count; i++) {
        if (strcmp(pa->entries[i].flag, flag) == 0)
            return *(int *)pa->entries[i].value;
    }
    return fallback;
}

int has_flag(const struct parsed_args *pa, const char *flag) {
    for (int i = 0; i < pa->count; i++) {
        if (strcmp(pa->entries[i].flag, flag) == 0)
            return 1;
    }
    return 0;
}

void free_parsed_args(struct parsed_args *pa) {
    if (!pa->entries)
        return;
    for (int i = 0; i < pa->count; i++) {
        free(pa->entries[i].flag);
        free(pa->entries[i].value);
    }
    free(pa->entries);
    pa->entries = NULL;
    pa->count   = 0;
}
