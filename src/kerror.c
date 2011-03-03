
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>

#include "klisp.h"
#include "kstate.h"

void klispE_throw(klisp_State *K, char *msg, bool can_cont)
{
    fprintf(stderr, "\n*ERROR*: %s\n", msg);
    K->error_can_cont = can_cont;
    longjmp(K->error_jb, 1);
}

/* TEMP: for throwing with extra msg info */
void klispE_throw_extra(klisp_State *K, char *msg, char *extra_msg, 
			bool can_cont) {
    fprintf(stderr, "\n*ERROR*: %s %s\n", msg, extra_msg);
    K->error_can_cont = can_cont;
    longjmp(K->error_jb, 1);
}
