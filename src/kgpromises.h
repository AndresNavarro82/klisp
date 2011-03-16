/*
** kgpromises.h
** Promises features for the ground environment
** See Copyright Notice in klisp.h
*/

#ifndef kgpromises_h
#define kgpromises_h

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kobject.h"
#include "klisp.h"
#include "kstate.h"
#include "kghelpers.h"

/* 9.1.1 promise? */
/* uses typep */

/* 9.1.2 force */
/* TODO */

/* 9.1.3 $lazy */
/* TODO */

/* 9.1.4 memoize */
void memoize(klisp_State *K, TValue *xparams, TValue ptree, TValue denv);

#endif
