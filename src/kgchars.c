/*
** kgchars.c
** Characters features for the ground environment
** See Copyright Notice in klisp.h
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "kstate.h"
#include "kobject.h"
#include "kapplicative.h"
#include "koperative.h"
#include "kcontinuation.h"
#include "kerror.h"

#include "kghelpers.h"
#include "kgchars.h"

/* 14.1.1? char? */
/* uses typep */

/* 14.1.2? char-alphabetic?, char-numeric?, char-whitespace? */
/* use ftyped_predp */

/* 14.1.3? char-upper-case?, char-lower-case? */
/* use ftyped_predp */

/* Helpers for typed predicates */
bool kcharp(TValue tv) { return ttischar(tv); }
bool kchar_alphabeticp(TValue ch) { return isalpha(chvalue(ch)) != 0; }
bool kchar_numericp(TValue ch)    { return isdigit(chvalue(ch)) != 0; }
bool kchar_whitespacep(TValue ch) { return isspace(chvalue(ch)) != 0; }
bool kchar_upper_casep(TValue ch) { return isupper(chvalue(ch)) != 0; }
bool kchar_lower_casep(TValue ch) { return islower(chvalue(ch)) != 0; }

/* 14.1.4? char->integer, integer->char */
void kchar_to_integer(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, "char->integer", ptree, "character", ttischar, ch);

    kapply_cc(K, i2tv((int32_t) chvalue(ch)));
}

/* TEMP: this should arbitrary integers (and throw an error if out of 
   range */
void kinteger_to_char(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, "integer->char", ptree, "finite integer", ttisfixint, itv);
    
    int32_t i = ivalue(itv);

    /* for now only allow ASCII */
    if (i < 0 || i > 127) {
	klispE_throw(K, "integer->char: integer out of ASCII range [0 - 127]");
	return;
    }
    kapply_cc(K, ch2tv((char) i));
}

/* 14.1.4? char-upcase, char-downcase */
/* TODO */

/* 14.2.1? char=? */
/* TODO */

/* 14.2.2? char<?, char<=?, char>?, char>=? */
/* TODO */

/* 14.2.3? char-ci=? */
/* TODO */

/* 14.2.4? char-ci<?, char-ci<=?, char-ci>?, char-ci>=? */
/* TODO */
