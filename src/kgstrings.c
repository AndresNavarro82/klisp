/*
** kgstrings.c
** Strings features for the ground environment
** See Copyright Notice in klisp.h
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
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
#include "ksymbol.h"
#include "kstring.h"

#include "kghelpers.h"
#include "kgchars.h" /* for kcharp */
#include "kgstrings.h"
#include "kgnumbers.h" /* for kintegerp & knegativep */

/* 13.1.1? string? */
/* uses typep */

/* 13.1.2? make-string */
void make_string(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_al1tp(K, ptree, "integer", kintegerp, tv_s, 
	       maybe_char);

    char fill = ' ';
    if (get_opt_tpar(K, "make-string", K_TCHAR, &maybe_char))
	fill = chvalue(maybe_char);

    if (knegativep(tv_s)) {
	klispE_throw_simple(K, "negative size");    
	return;
    } else if (!ttisfixint(tv_s)) {
	klispE_throw_simple(K, "size is too big");    
	return;
    }

    TValue new_str = kstring_new_sf(K, ivalue(tv_s), fill);
    kapply_cc(K, new_str);
}

/* 13.1.3? string-length */
void string_length(klisp_State *K, TValue *xparams, TValue ptree, 
		     TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, ptree, "string", ttisstring, str);

    TValue res = i2tv(kstring_size(str));
    kapply_cc(K, res);
}

/* 13.1.4? string-ref */
void string_ref(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_2tp(K, ptree, "string", ttisstring, str,
	     "integer", kintegerp, tv_i);

    if (!ttisfixint(tv_i)) {
	/* TODO show index */
	klispE_throw_simple(K, "index out of bounds");
	return;
    }
    int32_t i = ivalue(tv_i);
    
    if (i < 0 || i >= kstring_size(str)) {
	/* TODO show index */
	klispE_throw_simple(K, "index out of bounds");
	return;
    }

    TValue res = ch2tv(kstring_buf(str)[i]);
    kapply_cc(K, res);
}

/* 13.1.5? string-set! */
void string_setS(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_3tp(K, ptree, "string", ttisstring, str,
	     "integer", kintegerp, tv_i, "char", ttischar, tv_ch);

    if (!ttisfixint(tv_i)) {
	/* TODO show index */
	klispE_throw_simple(K, "index out of bounds");
	return;
    } else if (kstring_immutablep(str)) {
	klispE_throw_simple(K, "immutable string");
	return;
    }

    int32_t i = ivalue(tv_i);
    
    if (i < 0 || i >= kstring_size(str)) {
	/* TODO show index */
	klispE_throw_simple(K, "index out of bounds");
	return;
    }

    kstring_buf(str)[i] = chvalue(tv_ch);
    kapply_cc(K, KINERT);
}

/* Helper for string and list->string */
/* GC: Assumes ls is rooted */
inline TValue list_to_string_h(klisp_State *K, char *name, TValue ls)
{
    int32_t dummy;
    /* don't allow cycles */
    int32_t pairs = check_typed_list(K, name, "char", kcharp, false,
				     ls, &dummy);

    TValue new_str;
    /* the if isn't strictly necessary but it's clearer this way */
    if (pairs == 0) {
	return K->empty_string; 
    } else {
	new_str = kstring_new_s(K, pairs);
	char *buf = kstring_buf(new_str);
	TValue tail = ls;
	while(pairs--) {
	    *buf++ = chvalue(kcar(tail));
	    tail = kcdr(tail);
	}
	return new_str;
    }
}

/* 13.2.1? string */
void string(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    
    TValue new_str = list_to_string_h(K, "string", ptree);
    kapply_cc(K, new_str);
}

/* 13.2.2? string=?, string-ci=? */
/* use ftyped_bpredp */

/* 13.2.3? string<?, string<=?, string>?, string>=? */
/* use ftyped_bpredp */

/* 13.2.4? string-ci<?, string-ci<=?, string-ci>?, string-ci>=? */
/* use ftyped_bpredp */

/* Helpers for binary predicates */
/* XXX: this should probably be in file kstring.h */

bool kstring_eqp(TValue str1, TValue str2) { 
    return tv_equal(str1, str2) || kstring_equalp(str1, str2);
}

bool kstring_ci_eqp(TValue str1, TValue str2)
{
    int32_t size = kstring_size(str1);
    if (kstring_size(str2) != size)
	return false;
    else {
	char *buf1 = kstring_buf(str1);
	char *buf2 = kstring_buf(str2);

	while(size--) {
	    if (tolower(*buf1) != tolower(*buf2))
		return false;
	    buf1++, buf2++;
	}
	return true;
    }
}

bool kstring_ltp(TValue str1, TValue str2)
{
    int32_t size1 = kstring_size(str1);
    int32_t size2 = kstring_size(str2);

    int32_t min_size = size1 < size2? size1 : size2;
    /* memcmp > 0 if str1 has a bigger char in first diff position */
    int res = memcmp(kstring_buf(str1), kstring_buf(str2), min_size);

    return (res < 0 || (res == 0 && size1 < size2));
}

bool kstring_lep(TValue str1, TValue str2) { return !kstring_ltp(str2, str1); }
bool kstring_gtp(TValue str1, TValue str2) { return kstring_ltp(str2, str1); }
bool kstring_gep(TValue str1, TValue str2) { return !kstring_ltp(str1, str2); }

bool kstring_ci_ltp(TValue str1, TValue str2)
{
    int32_t size1 = kstring_size(str1);
    int32_t size2 = kstring_size(str2);
    int32_t min_size = size1 < size2? size1 : size2;
    char *buf1 = kstring_buf(str1);
    char *buf2 = kstring_buf(str2);

    while(min_size--) {
	int diff = (int) tolower(*buf1) - (int) tolower(*buf2);
	if (diff > 0)
	    return false;
	else if (diff < 0)
	    return true;
	buf1++, buf2++;
    }
    return size1 < size2;
}

bool kstring_ci_lep(TValue str1, TValue str2)
{
    return !kstring_ci_ltp(str2, str1);
}

bool kstring_ci_gtp(TValue str1, TValue str2)
{
    return kstring_ci_ltp(str2, str1);
}

bool kstring_ci_gep(TValue str1, TValue str2)
{
    return !kstring_ci_ltp(str1, str2);
}

/* 13.2.5? substring */
/* TEMP: at least for now this always returns mutable strings */
void substring(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_3tp(K, ptree, "string", ttisstring, str,
	     "integer", kintegerp, tv_start,
	     "integer", kintegerp, tv_end);

    if (!ttisfixint(tv_start) || ivalue(tv_start) < 0 ||
	  ivalue(tv_start) > kstring_size(str)) {
	/* TODO show index */
	klispE_throw_simple(K, "start index out of bounds");
	return;
    } 

    int32_t start = ivalue(tv_start);

    if (!ttisfixint(tv_end) || ivalue(tv_end) < 0 || 
	  ivalue(tv_end) > kstring_size(str)) {
	klispE_throw_simple(K, "end index out of bounds");
	return;
    }

    int32_t end = ivalue(tv_end);

    if (start > end) {
	/* TODO show indexes */
	klispE_throw_simple(K, "end index is smaller than start index");
	return;
    }

    int32_t size = end - start;
    TValue new_str;
    /* the if isn't strictly necessary but it's clearer this way */
    if (size == 0) {
	new_str = K->empty_string;
    } else {
	new_str = kstring_new_bs(K, kstring_buf(str)+start, size);
    }
    kapply_cc(K, new_str);
}

/* 13.2.6? string-append */
/* TEMP: at least for now this always returns mutable strings */
/* TEMP: this does 3 passes over the list */
void string_append(klisp_State *K, TValue *xparams, TValue ptree, 
		   TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    int32_t dummy;
    /* don't allow cycles */
    int32_t pairs = check_typed_list(K, "string-append", "string", kstringp, 
				     false, ptree, &dummy);

    TValue new_str;
    int64_t total_size = 0; /* use int64 to check for overflow */
    /* the if isn't strictly necessary but it's clearer this way */
    int32_t saved_pairs = pairs; /* save pairs for next loop */
    TValue tail = ptree;
    while(pairs--) {
	total_size += kstring_size(kcar(tail));
	if (total_size > INT32_MAX) {
	    klispE_throw_simple(K, "resulting string is too big");
	    return;
	}
	tail = kcdr(tail);
    }
    /* this is safe */
    int32_t size = (int32_t) total_size;

    if (size == 0) {
	new_str = K->empty_string; 
    } else {
	new_str = kstring_new_s(K, size);
	char *buf = kstring_buf(new_str);
	/* loop again to copy the chars of each string */
	tail = ptree;
	pairs = saved_pairs;

	while(pairs--) {
	    TValue first = kcar(tail);
	    int32_t first_size = kstring_size(first);
	    memcpy(buf, kstring_buf(first), first_size);
	    buf += first_size;
	    tail = kcdr(tail);
	}
    }

    kapply_cc(K, new_str);
}


/* 13.2.7? string->list, list->string */
void string_to_list(klisp_State *K, TValue *xparams, TValue ptree, 
		    TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    
    bind_1tp(K, ptree, "string", ttisstring, str);
    int32_t pairs = kstring_size(str);
    char *buf = kstring_buf(str);

    TValue tail = kget_dummy1(K);

    while(pairs--) {
	TValue new_pair = kcons(K, ch2tv(*buf), KNIL);
	buf++;
	kset_cdr(tail, new_pair);
	tail = new_pair;
    }
    kapply_cc(K, kcutoff_dummy1(K));
}

void list_to_string(klisp_State *K, TValue *xparams, TValue ptree, 
		    TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    
    /* check later in list_to_string_h */
    bind_1p(K, ptree, ls);

    TValue new_str = list_to_string_h(K, "list->string", ls);
    kapply_cc(K, new_str);
}

/* 13.2.8? string-copy */
/* TEMP: at least for now this always returns mutable strings */
void string_copy(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, ptree, "string", ttisstring, str);

    TValue new_str;
    /* the if isn't strictly necessary but it's clearer this way */
    if (tv_equal(str, K->empty_string)) {
	new_str = str; 
    } else {
	new_str = kstring_new_bs(K, kstring_buf(str), kstring_size(str));
    }
    kapply_cc(K, new_str);
}

/* 13.2.9? string->immutable-string */
void string_to_immutable_string(klisp_State *K, TValue *xparams, 
				TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, ptree, "string", ttisstring, str);

    TValue res_str;
    if (kstring_immutablep(str)) {/* this includes the empty list */
	res_str = str;
    } else {
	res_str = kstring_new_bs_imm(K, kstring_buf(str), kstring_size(str));
    }
    kapply_cc(K, res_str);
}

/* 13.2.10? string-fill! */
void string_fillS(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_2tp(K, ptree, "string", ttisstring, str,
	     "char", ttischar, tv_ch);

    if (kstring_immutablep(str)) {
	klispE_throw_simple(K, "immutable string");
	return;
    } 

    memset(kstring_buf(str), chvalue(tv_ch), kstring_size(str));
    kapply_cc(K, KINERT);
}


/* 13.3.1? symbol->string */
/* The strings in symbols are immutable so we can just return that */
void symbol_to_string(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, ptree, "symbol", ttissymbol, sym);
    TValue str = ksymbol_str(sym);
    kapply_cc(K, str);
}

/* 13.3.2? string->symbol */
/* TEMP: for now this can create symbols with no external representation
   this includes all symbols with non identifiers characters.
*/
/* NOTE:
   Symbols with uppercase alphabetic characters will write as lowercase and
   so, when read again will not compare as either eq? or equal?. This is ok
   because the report only says that read objects when written and read 
   again must be equal? which happens here 
*/
/* If the string is mutable it is copied */
void string_to_symbol(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    bind_1tp(K, ptree, "string", ttisstring, str);
    /* TODO si */
    TValue new_sym = ksymbol_new_check_i(K, str, KNIL);
    kapply_cc(K, new_sym);
}
