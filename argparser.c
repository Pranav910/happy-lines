#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct arguments {
    void *value;
    char *flag;
};

int get_flag_index(char *argv) {
    for (int i = 2; i < strlen(argv); i++) {
        if (argv[i] == '=') {
            return i;
        }
    }
    return 0;
}

int* get_flag_value(char *argv, int flag_index) {
    int* flag_value = (int*)malloc(sizeof(int));
    *flag_value = atoi(argv + flag_index + 1);
    return flag_value;
}

struct arguments* parse_arguments(int argc, char *argv[]) {
    struct arguments* arguments = (struct arguments*)malloc(sizeof(struct arguments) * argc);

    if (argc < 2) {
      free(arguments);
      return NULL;
    }

    for (int i = 1; i < argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] == '-') {
            int flag_index = get_flag_index(argv[i]);
            int flag_length = flag_index - 1;
            arguments[i - 1].flag = (char*)malloc(sizeof(char) * flag_length - 1);
            strncpy(arguments[i - 1].flag, argv[i] + 2, flag_length - 1);
            arguments[i - 1].flag[flag_length - 1] = '\0';
            arguments[i - 1].value = (int*)get_flag_value(argv[i], flag_index);
        }
    }

    // printf("argc: %d\n", argc);

    // for (int i = 1; i < argc; i++) {
    //     printf("Flag: %s\n", arguments[i].flag);
    //     printf("Value: %d\n", *(int*)arguments[i].value);
    // }

  return arguments;
}