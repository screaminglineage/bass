#ifndef BASS_UTILS_H
#define BASS_UTILS_H
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define dyn_append(da, item)                                                      \
do {                                                                         \
    if ((da)->size == (da)->capacity) {                                        \
        (da)->capacity = ((da)->capacity <= 0) ? 1 : (da)->capacity * 2;         \
        (da)->data = realloc((da)->data, (da)->capacity * sizeof(*(da)->data));  \
        assert((da)->data && "Catastrophic Failure: Allocation failed!");        \
    }                                                                          \
    (da)->data[(da)->size++] = item;                                           \
} while (0)

#define MODULO(a, b) (((a) % (b)) + (b)) % (b);

typedef struct {
    const char *data;
    size_t length;
} StringView;


// Use to print the StringView along with `%.*s` in the format string
#define SV_FORMAT(sv) (int)(sv).length, (sv).data


// Make sure that the C-String isnt NULL
static inline bool string_view_cstring_eq(StringView a, const char *b) {
    size_t i = 0;
    while (b[i] && i < a.length) {
        if (b[i] != a.data[i]) return false;
        i++;
    }
    return !b[i] && (i == a.length);
}

static inline char *string_view_to_cstring(StringView sv) {
    char *str =  malloc(sizeof(char)*(sv.length + 1));
    memcpy(str, sv.data, sv.length);
    str[sv.length] = 0;
    return str;
}

static inline bool string_view_eq(StringView a, StringView b) {
    if (a.length != b.length) return false;
    return (strncmp(a.data, b.data, a.length) == 0);
}

static inline bool read_to_string(const char *filepath, StringView *sv) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "bass: failed to open source file `%s`: %s\n", filepath, strerror(errno));
        return false;
    }
    fseek(file, 0, SEEK_END);
    long ret = ftell(file);
    if (ret == -1) {
        fprintf(stderr, "bass: couldnt read file position in `%s`: %s\n", filepath, strerror(errno));
        return false;
    }
    // ret cannot be negative at this point
    size_t size = ret;
    rewind(file);

    char *data = malloc(size + 1);
    if (!data) {
        return false;
    }
    fread(data, size, 1, file);
    fclose(file);

    data[size] = 0;
    *sv = (StringView){data, size - 1};
    return true;
}


#endif
