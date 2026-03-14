#include "argparser.h"
#include <stdlib.h>
#include <string.h>

static int find_equals(const char *arg) {
    for (int i = 2; arg[i] != '\0'; i++) {
        if (arg[i] == '=')
            return i;
    }
    return -1;
}

struct parsed_args parse_arguments(int argc, char *argv[]) {
    struct parsed_args result = { NULL, 0 };

    if (argc < 2)
        return result;

    result.entries = (struct argument *)malloc(sizeof(struct argument) * (argc - 1));
    if (!result.entries)
        return result;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-')
            continue;

        int idx = result.count;
        int eq  = find_equals(argv[i]);

        if (eq > 0) {
            int flag_len = eq - 2;
            result.entries[idx].flag = (char *)malloc(flag_len + 1);
            if (!result.entries[idx].flag) continue;
            strncpy(result.entries[idx].flag, argv[i] + 2, flag_len);
            result.entries[idx].flag[flag_len] = '\0';

            int *val = (int *)malloc(sizeof(int));
            if (!val) { free(result.entries[idx].flag); continue; }
            *val = atoi(argv[i] + eq + 1);
            result.entries[idx].value = val;
            result.entries[idx].type  = ARG_INT;
        } else {
            int flag_len = (int)strlen(argv[i]) - 2;
            result.entries[idx].flag = (char *)malloc(flag_len + 1);
            if (!result.entries[idx].flag) continue;
            strncpy(result.entries[idx].flag, argv[i] + 2, flag_len);
            result.entries[idx].flag[flag_len] = '\0';

            int *val = (int *)malloc(sizeof(int));
            if (!val) { free(result.entries[idx].flag); continue; }
            *val = 1;
            result.entries[idx].value = val;
            result.entries[idx].type  = ARG_BOOL;
        }
        result.count++;
    }

    return result;
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
