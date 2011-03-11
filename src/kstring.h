/*
** kstring.h
** Kernel Strings
** See Copyright Notice in klisp.h
*/

#ifndef kstring_h
#define kstring_h

#include <stdbool.h>

#include "kobject.h"
#include "kstate.h"

/* TEMP: for now all strings are mutable */

TValue kstring_new_empty(klisp_State *K);
TValue kstring_new(klisp_State *K, const char *buf, uint32_t size);
#define kstring_buf(tv_) (((String *) ((tv_).tv.v.gc))->b)
#define kstring_size(tv_) (((String *) ((tv_).tv.v.gc))->size)

#define kstring_is_empty(tv_) (kstring_size(tv_) == 0)

/* both obj1 and obj2 should be strings! */
bool kstring_equalp(TValue obj1, TValue obj2);

#endif
