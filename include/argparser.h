#ifndef HL_ARGPARSER_H
#define HL_ARGPARSER_H

enum arg_type { ARG_INT, ARG_BOOL };

struct argument {
    char *flag;
    void *value;
    enum arg_type type;
};

struct parsed_args {
    struct argument *entries;
    int count;
};

struct parsed_args parse_arguments(int argc, char *argv[]);
int                get_int_arg(const struct parsed_args *pa, const char *flag,
                               int fallback);
int                has_flag(const struct parsed_args *pa, const char *flag);
void               free_parsed_args(struct parsed_args *pa);

#endif /* HL_ARGPARSER_H */
