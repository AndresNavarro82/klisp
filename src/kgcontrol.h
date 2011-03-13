/*
** kgcontrol.h
** Control features for the ground environment
** See Copyright Notice in klisp.h
*/

#ifndef kgcontrol_h
#define kgcontrol_h

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kobject.h"
#include "klisp.h"
#include "kstate.h"
#include "kghelpers.h"

/* Helper (also used by $vau and $lambda) */
void do_seq(klisp_State *K, TValue *xparams, TValue obj);

/* 4.5.1 inert? */
/* uses typep */

/* 4.5.2 $if */

void Sif(klisp_State *K, TValue *xparams, TValue ptree, TValue denv);

/* 5.1.1 $sequence */
void Ssequence(klisp_State *K, TValue *xparams, TValue ptree, TValue denv);

/* 5.6.1 $cond */
/* TODO */

#endif
