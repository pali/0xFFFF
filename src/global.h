
#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int simulate;
extern int noverify;
extern int verbose;

#define VERBOSE(format, ...) do { if ( verbose ) { fprintf(stderr, format, ##__VA_ARGS__); } } while (0)
#define ERROR(errno, format, ...) do { fprintf(stderr, "Error: "); fprintf(stderr, format, ##__VA_ARGS__); if ( errno ) fprintf(stderr, ": %s\n", strerror(errno)); else fprintf(stderr, "\n"); } while (0)
#define WARNING(format, ...) do { fprintf(stderr, "Warning: "); fprintf(stderr, format, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)

#define ALLOC_ERROR() do { ERROR(0, "Cannot allocate memory"); } while (0)
#define ALLOC_ERROR_RETURN(...) do { ALLOC_ERROR(); return __VA_ARGS__; } while (0)

#endif
