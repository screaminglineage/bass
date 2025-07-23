#ifndef BASS_CONSTANTS_H
#define BASS_CONSTANTS_H

#define KB 1024
#define MB 1024*KB

#define REG_COUNT 8
#define STACK_MAX (2*KB)
#define MEMORY_SIZE (4*MB)
#define MAX_OPERANDS 3
#define DEFAULT_COMPILER_OUTPUT "bass-compiled"

// TODO: document this label as reserved along with `_start`
#define ENTRY_POINT_NAME "__bass_main"

#endif
