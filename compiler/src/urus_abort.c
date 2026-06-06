#include "urus_abort.h"
#include <stdio.h>
#include <stdlib.h>

static jmp_buf *g_abort_jb = NULL;

void urus_abort_arm(jmp_buf *jb)  { g_abort_jb = jb; }
void urus_abort_disarm(void)      { g_abort_jb = NULL; }

void urus_abort_oom(const char *msg) {
    fprintf(stderr, "urus: %s\n", msg ? msg : "out of memory");
    if (g_abort_jb) longjmp(*g_abort_jb, 1);
    exit(1);
}
