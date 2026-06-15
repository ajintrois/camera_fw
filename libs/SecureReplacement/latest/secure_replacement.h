#ifndef SECURE_REPLACEMENT_H
#define SECURE_REPLACEMENT_H

#include <stdio.h>
#include <stddef.h>

/* This block tells C++ compilers to use C-style linking */
#ifdef __cplusplus
extern "C" {
#endif

const char* libsafec_version();

/* String and Memory Safety */
size_t safe_strlen(const char *str, size_t max_len);
char* safe_strncpy(char* dest, const char* src, size_t count);
int    safe_memcpy(void *dest, size_t destsz, const void *src, size_t count);

/* Input and Conversion Safety */
int    safe_snprintf(char *buf, size_t buf_size, const char *fmt, ...);
int    safe_atoi(const char *nptr, int *value);
int    safe_fgets(char *s, size_t maxbufsz, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_REPLACEMENT_H */