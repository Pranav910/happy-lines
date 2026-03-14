#include "contributor.h"
#include "platform.h"

#define MAX_CONTRIBUTORS 256
#define MAX_NAME_LEN     256

struct contributor {
    char name[MAX_NAME_LEN];
    long added;
    long removed;
};

/* ─── Internal Helpers ─── */

static int get_contributors(struct contributor *out, int max) {
    FILE *fp = hl_popen("git log --format=\"%aN\"", "r");
    if (!fp) {
        fprintf(stderr, "Error: failed to retrieve git authors\n");
        return 0;
    }

    int  count = 0;
    char name[MAX_NAME_LEN];

    while (fgets(name, sizeof(name), fp) && count < max) {
        name[strcspn(name, "\n\r")] = '\0';
        if (name[0] == '\0') continue;

        int duplicate = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(out[i].name, name) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) continue;

        strncpy(out[count].name, name, MAX_NAME_LEN - 1);
        out[count].name[MAX_NAME_LEN - 1] = '\0';
        out[count].added   = 0;
        out[count].removed = 0;
        count++;
    }

    hl_pclose(fp);
    return count;
}

static void compute_stats(struct contributor *c) {
    char escaped[MAX_NAME_LEN * 2];
    size_t j = 0;
    for (size_t i = 0; c->name[i] && j < sizeof(escaped) - 2; i++) {
        if (c->name[i] == '"' || c->name[i] == '\\')
            escaped[j++] = '\\';
        escaped[j++] = c->name[i];
    }
    escaped[j] = '\0';

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "git log --author=\"%s\" --pretty=tformat: --numstat", escaped);

    FILE *fp = hl_popen(cmd, "r");
    if (!fp) return;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        long add, rem;
        char file[512];
        if (sscanf(line, "%ld\t%ld\t%511s", &add, &rem, file) == 3) {
            c->added   += add;
            c->removed += rem;
        }
    }
    hl_pclose(fp);
}

static int cmp_by_net_desc(const void *a, const void *b) {
    long net_a = ((const struct contributor *)a)->added -
                 ((const struct contributor *)a)->removed;
    long net_b = ((const struct contributor *)b)->added -
                 ((const struct contributor *)b)->removed;
    return (net_b > net_a) - (net_b < net_a);
}

static void print_table(struct contributor *list, int count) {
    const char *div =
        "--------------------------------------------------------------";

    printf("\n%s\n", div);
    printf(" %-30s %9s %9s %9s\n", "Contributor", "Added", "Removed", "Net");
    printf("%s\n", div);

    long total_add = 0, total_rem = 0;
    for (int i = 0; i < count; i++) {
        printf(" %-30s %9ld %9ld %9ld\n",
               list[i].name, list[i].added, list[i].removed,
               list[i].added - list[i].removed);
        total_add += list[i].added;
        total_rem += list[i].removed;
    }

    printf("%s\n", div);
    printf(" %-30s %9ld %9ld %9ld\n",
           "TOTAL", total_add, total_rem, total_add - total_rem);
    printf("%s\n", div);
}

/* ─── Public API ─── */

void run_contributor_analysis(void) {
    printf("\nAnalyzing git contributions...\n");

    struct contributor *contributors =
        (struct contributor *)calloc(MAX_CONTRIBUTORS,
                                     sizeof(struct contributor));
    if (!contributors) {
        fprintf(stderr, "Error: out of memory\n");
        return;
    }

    int count = get_contributors(contributors, MAX_CONTRIBUTORS);
    if (count == 0) {
        printf("No contributors found.\n");
        free(contributors);
        return;
    }

    for (int i = 0; i < count; i++)
        compute_stats(&contributors[i]);

    qsort(contributors, count, sizeof(struct contributor), cmp_by_net_desc);
    print_table(contributors, count);
    free(contributors);
}
