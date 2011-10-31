/*
** kgerror.h
** Error handling features for the ground environment
** See Copyright Notice in klisp.h
*/

#ifndef kgerror_h
#define kgerror_h

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kobject.h"
#include "klisp.h"
#include "kstate.h"
#include "kghelpers.h"

/* init ground */
void kinit_error_ground_env(klisp_State *K);

#endif
