/*
** kgnumbers.c
** Numbers features for the ground environment
** See Copyright Notice in klisp.h
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kstate.h"
#include "kobject.h"
#include "kapplicative.h"
#include "koperative.h"
#include "kcontinuation.h"
#include "kerror.h"
#include "ksymbol.h"

#include "kghelpers.h"
#include "kgnumbers.h"

/* 15.5.1? number?, finite?, integer? */
/* use ftypep & ftypep_predp */

/* Helpers for typed predicates */
bool knumberp(TValue obj) { return ttype(obj) <= K_LAST_NUMBER_TYPE; }
/* obj is known to be a number */
bool kfinitep(TValue obj) { return (!ttiseinf(obj) && !ttisiinf(obj)); }
/* TEMP: for now only fixint, should also include bigints and 
   inexact integers */
bool kintegerp(TValue obj) { return ttisfixint(obj); }

/* 12.5.2 =? */
/* uses typed_bpredp */

/* 12.5.3 <?, <=?, >?, >=? */
/* use typed_bpredp */

/* Helpers for typed binary predicates */
/* XXX: this should probably be in a file knumber.h but there is no real need for 
   that file yet */

/* this will come handy when there are more numeric types,
   it is intended to be used in switch */
inline int32_t max_ttype(TValue obj1, TValue obj2)
{
    int32_t t1 = ttype(obj1);
    int32_t t2 = ttype(obj2);

    return (t1 > t2? t1 : t2);
}

/* TEMP: for now only fixints and exact infinities */
bool knum_eqp(TValue n1, TValue n2) { return tv_equal(n1, n2); }
bool knum_ltp(TValue n1, TValue n2) 
{ 
    switch(max_ttype(n1, n2)) {
    case K_TFIXINT:
	return ivalue(n1) < ivalue(n2);
    case K_TEINF:
	return !tv_equal(n1, n2) && (tv_equal(n1, KEMINF) ||
				     tv_equal(n2, KEPINF));
    default:
	/* shouldn't happen */
	assert(0);
	return false;
    }
}

bool knum_lep(TValue n1, TValue n2)
{ 
    switch(max_ttype(n1, n2)) {
    case K_TFIXINT:
	return ivalue(n1) <= ivalue(n2);
    case K_TEINF:
	return tv_equal(n1, n2) || tv_equal(n1, KEMINF) || 
	    tv_equal(n2, KEPINF);
    default:
	/* shouldn't happen */
	assert(0);
	return false;
    }
}

bool knum_gtp(TValue n1, TValue n2)
{ 
    switch(max_ttype(n1, n2)) {
    case K_TFIXINT:
	return ivalue(n1) > ivalue(n2);
    case K_TEINF:
	return !tv_equal(n1, n2) && (tv_equal(n1, KEPINF) ||
				     tv_equal(n2, KEMINF));
    default:
	/* shouldn't happen */
	assert(0);
	return false;
    }
}

bool knum_gep(TValue n1, TValue n2)
{ 
    switch(max_ttype(n1, n2)) {
    case K_TFIXINT:
	return ivalue(n1) >= ivalue(n2);
    case K_TEINF:
	return tv_equal(n1, n2) || tv_equal(n1, KEPINF) ||
	    tv_equal(n2, KEMINF);
    default:
	/* shouldn't happen */
	assert(0);
	return false;
    }
}

/*
** REFACTOR: all of *, -, and + should be refactored
** this will probably happen when bignums are introduced
** the idea is to have generic binary +, -, * and /, maybe
** inlineable. That would clear up all the border cases
** like infinities that are obscuring the code.
**/

/* 12.5.4 + */
void kplus(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(denv);
    UNUSED(xparams);
    /* cycles are allowed, loop counting pairs */
    int32_t cpairs; 
    int32_t pairs = check_typed_list(K, "+", "number", knumberp, true,
				     ptree, &cpairs);
    int32_t apairs = pairs - cpairs;

    TValue res;

    /* first the acyclic part */
    TValue ares = i2tv(0);
    int32_t accum = 0;
    bool seen_infinity = false;
    TValue tail = ptree;

    while(apairs--) {
	TValue first = kcar(tail);
	tail = kcdr(tail);

	if (ttiseinf(first)) {
	    if (seen_infinity && !tv_equal(first, ares)) {
		/* report: #e+infinity + #e-infinity has no primary value */
		klispE_throw(K, "+: result has no primary value");
		return;
	    } else {
		/* record which infinity we have seen */
		seen_infinity = true;
		ares = first;
	    }
	} else if (!seen_infinity) {
	    accum += ivalue(first);
	}
    }

    if (!seen_infinity)
	ares = i2tv(accum);

    /* next the cyclic part */
    TValue cres = i2tv(0);

    if (cpairs == 0) {
	res = ares;
    } else {
	bool all_zero = true;

	seen_infinity = false;
	accum = 0;

	while(cpairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);

	    all_zero = all_zero && kfast_zerop(first);

	    if (ttiseinf(first)) {
		if (seen_infinity && !tv_equal(first, cres)) {
		    /* report: #e+infinity + #e-infinity has no primary value */
		    klispE_throw(K, "+: result has no primary value");
		    return;
		} else {
		    /* record which infinity we have seen */
		    seen_infinity = true;
		    cres = first;
		}
	    } else if (!seen_infinity) {
		accum += ivalue(first);
	    }
	}

	if (!seen_infinity) {
	    if (accum == 0)  {
		if (!all_zero) {
		    /* report */
		    klispE_throw(K, "+: result has no primary value");
		    return;
		} else {
		    cres = i2tv(accum);
		}
	    } else {
		cres = accum < 0? KEMINF : KEPINF;
	    }
	}

	if (ttiseinf(ares)) {
	    if (!ttiseinf(cres) || tv_equal(ares, cres))
		res = ares;
	    else {
		/* report */
		klispE_throw(K, "+: result has no primary value");
		return;
	    }
	} else {
	    if (ttiseinf(cres))
		res = cres;
	    else 
		res = i2tv(ivalue(ares) + ivalue(cres));
	}
    } 
    kapply_cc(K, res);
}


/* 12.5.5 * */
void ktimes(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(denv);
    UNUSED(xparams);
    /* cycles are allowed, loop counting pairs */
    int32_t cpairs; 
    int32_t pairs = check_typed_list(K, "*", "number", knumberp, true,
				     ptree, &cpairs);
    int32_t apairs = pairs - cpairs;

    TValue res;

    /* first the acyclic part */
    TValue ares = i2tv(1);
    int32_t accum = 1;
    bool seen_zero = false;
    TValue tail = ptree;
    bool seen_infinity = false;

    while(apairs--) {
	TValue first = kcar(tail);
	tail = kcdr(tail);

	if (ttiseinf(first)) {
	    if (seen_zero) {
		/* report: #e+infinity * 0 has no primary value */
		klispE_throw(K, "*: result has no primary value");
		return;
	    } else {
		/* record which infinity we have seen */
		if (!seen_infinity) {
		    seen_infinity = true;
		    ares = first;
		} else if (tv_equal(first, KEMINF))
		    ares = kneg_inf(ares);
	    }
	} else if (ivalue(first) == 0) {
	    if (seen_infinity) {
		/* report: #e+infinity * 0 has no primary value */
		klispE_throw(K, "*: result has no primary value");
		return;
	    }
	    seen_zero = true;
	    accum = 0;
	} else if (!seen_zero) {
	    accum *= ivalue(first);
	}
    }

    if (seen_infinity)
	ares = (accum < 0)? kneg_inf(ares) : ares;
    else
	ares = i2tv(accum);

    /* next the cyclic part */
    TValue cres = i2tv(1);

    if (cpairs == 0) {
	res = ares;
    } else {
	bool all_one = true;
	seen_zero = false;
	seen_infinity = false;
	accum = 1;

	while(cpairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);

	    all_one = all_one && kfast_onep(first);

	    if (ttiseinf(first)) {
		if (seen_zero) {
		    /* report: 0 * #e+infinity has no primary value */
		    klispE_throw(K, "*: result has no primary value");
		    return;
		} else {
		    /* record which infinity we have seen */
		    if (!seen_infinity) {
			seen_infinity = true;
			cres = first;
		    } else if (tv_equal(first, KEMINF))
			cres = kneg_inf(cres);
		}
	    } else if (kfast_zerop(first)) {
		if (seen_infinity) {
		    /* report: 0 * #e+infinity has no primary value */
		    klispE_throw(K, "*: result has no primary value");
		    return;
		}
		seen_zero = true;
		accum = 0;
	    } else if (!seen_zero) {
		accum *= ivalue(first);
	    }
	}

	/* think of accum as the product of an infinite series */
	if (seen_infinity) {
	    cres = (accum < 0)? kneg_inf(cres) : cres;
	} else if (seen_zero || (accum >= 0 && accum < 1)) {
	    cres = i2tv(0);
	} else if (accum == 1) {
	    if (all_one)
		cres = i2tv(1);
	    else {
		klispE_throw(K, "*: result has no primary value");
		return;
	    }
	} else if (accum > 1) {
	    /* ASK JOHN: this is as per the report, but maybe we should check
	       that all elements are positive... */
	    cres = KEPINF;
	} else {
	    klispE_throw(K, "*: result has no primary value");
	    return;
	}

	if (ttiseinf(ares)) {
	    if (ttiseinf(cres)) {
		res = tv_equal(cres, ares)? KEPINF : KEMINF;
	    } else if (ivalue(cres) == 0) {
		klispE_throw(K, "*: result has no primary value");
		return;
	    } else {
		res = ivalue(cres) < 0? kneg_inf(ares) : ares;
	    }
	} else {
	    if (ttiseinf(cres)) {
		if (ivalue(ares) == 0) {
		    klispE_throw(K, "*: result has no primary value");
		    return;
		} else 
		    res = ivalue(ares) < 0? kneg_inf(cres) : cres;
	    } else {
		res = i2tv(ivalue(ares) * ivalue(cres));
	    }
	}
    } 
    kapply_cc(K, res);
}

/* 12.5.6 - */
void kminus(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(denv);
    UNUSED(xparams);
    /* cycles are allowed, loop counting pairs */
    int32_t cpairs;
    
    /* - in kernel (and unlike in scheme) requires at least 2 arguments */
    if (!ttispair(ptree) || !ttispair(kcdr(ptree))) {
	klispE_throw(K, "-: at least two values are required");
	return;
    } else if (!knumberp(kcar(ptree))) {
	klispE_throw(K, "-: bad type on first argument (expected number)");
	return;
    }
    TValue first_val = kcar(ptree);

    int32_t pairs = check_typed_list(K, "-", "number", knumberp, true,
				     kcdr(ptree), &cpairs);
    int32_t apairs = pairs - cpairs;

    TValue res;

    /* first the acyclic part */
    TValue ares = i2tv(0);
    int32_t accum = 0;
    bool seen_infinity = false;
    TValue tail = kcdr(ptree);

    while(apairs--) {
	TValue first = kcar(tail);
	tail = kcdr(tail);

	if (ttiseinf(first)) {
	    if (seen_infinity && !tv_equal(first, ares)) {
		/* report: #e+infinity + #e-infinity has no primary value */
		klispE_throw(K, "-: result has no primary value");
		return;
	    } else {
		/* record which infinity we have seen */
		seen_infinity = true;
		ares = first;
	    }
	} else if (!seen_infinity) {
	    accum += ivalue(first);
	}
    }

    if (!seen_infinity)
	ares = i2tv(accum);

    /* next the cyclic part */
    TValue cres = i2tv(0);

    if (cpairs == 0) {
	res = ares;
    } else {
	bool all_zero = true;

	seen_infinity = false;
	accum = 0;

	while(cpairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);

	    all_zero = all_zero && kfast_zerop(first);

	    if (ttiseinf(first)) {
		if (seen_infinity && !tv_equal(first, cres)) {
		    /* report: #e+infinity + #e-infinity has no primary value */
		    klispE_throw(K, "-: result has no primary value");
		    return;
		} else {
		    /* record which infinity we have seen */
		    seen_infinity = true;
		    cres = first;
		}
	    } else if (!seen_infinity) {
		accum += ivalue(first);
	    }
	}

	if (!seen_infinity) {
	    if (accum == 0)  {
		if (!all_zero) {
		    /* report */
		    klispE_throw(K, "-: result has no primary value");
		    return;
		} else {
		    cres = i2tv(accum);
		}
	    } else {
		cres = accum < 0? KEMINF : KEPINF;
	    }
	}

	if (ttiseinf(ares)) {
	    if (!ttiseinf(cres) || tv_equal(ares, cres))
		res = ares;
	    else {
		/* report */
		klispE_throw(K, "-: result has no primary value");
		return;
	    }
	} else {
	    if (ttiseinf(cres))
		res = cres;
	    else 
		res = i2tv(ivalue(ares) + ivalue(cres));
	}
    } 

    /* now substract the sum of all the elements in the list to the first 
       value */
    if (ttiseinf(first_val)) {
	if (!ttiseinf(res) || !tv_equal(first_val, res)) {
	    res = first_val;
	} else {
	    /* report */
	    klispE_throw(K, "-: result has no primary value");
	    return;
	}
    } else {
	if (ttiseinf(res))
	    res = kneg_inf(res);
	else 
	    res = i2tv(ivalue(first_val) - ivalue(res));
    }

    kapply_cc(K, res);
}

/* 12.5.7 zero? */
/* uses ftyped_predp */

/* Helper for zero? */
bool kzerop(TValue n) { return kfast_zerop(n); }

/* 12.5.8 div, mod, div-and-mod */
/* use div_mod */

/* 12.5.9 div0, mod0, div0-and-mod0 */
/* use div_mod */

/* Helpers for div, mod, div0 and mod0 */

/* zerop signals div0 and mod0 if true div and mod if false */
inline void div_mod(bool zerop, int32_t n, int32_t d, int32_t *div, 
		    int32_t *mod) 
{
    *div = n / d;
    *mod = n % d;

    if (zerop) {
	/* div0, mod0 or div-and-mod0 */
	/* -|d/2| <= mod0 < |d/2| */

	int32_t dabs = ((d<0? -d : d) + 1) / 2;
	
	if (*mod < -dabs) {
	    if (d < 0) {
		*mod -= d;
		++(*div);
	    } else {
		*mod += d;
		--(*div);
	    }
	} else if (*mod >= dabs) {
	    if (d < 0) {
		*mod += d;
		--(*div);
	    } else {
		*mod -= d;
		++(*div);
	    }
	}
    } else {
	/* div, mod or div-and-mod */
	/* 0 <= mod0 < |d| */
	if (*mod < 0) {
	    if (d < 0) {
		*mod -= d;
		++(*div);
	    } else {
		*mod += d;
		--(*div);
	    }
	}
    }
}

/* flags are FDIV_DIV, FDIV_MOD, FDIV_ZERO */
void kdiv_mod(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    /*
    ** xparams[0]: name symbol
    ** xparams[1]: div_mod_flags
    */
    char *name = ksymbol_buf(xparams[0]);
    int32_t flags = ivalue(xparams[1]);

    UNUSED(denv);

    bind_2tp(K, name, ptree, "number", knumberp, tv_n,
	     "number", knumberp, tv_d);

    /* TEMP: only fixnums */
    TValue tv_div, tv_mod;

    if (kfast_zerop(tv_d)) {
	klispE_throw_extra(K, name, ": division by zero");
	return;
    } else if (ttiseinf(tv_n)) {
	klispE_throw_extra(K, name, ": non finite dividend");
	return;
    } else if (ttiseinf(tv_d)) {
	tv_div = ((ivalue(tv_n) ^ ivalue(tv_d)) < 0)? KEPINF : KEMINF;
	tv_mod = i2tv(0);
    } else {
	int32_t div, mod;
	div_mod(flags & FDIV_ZERO, ivalue(tv_n), ivalue(tv_d), &div, &mod);
	tv_div = i2tv(div);
	tv_mod = i2tv(mod);
    }
    TValue res;
    if (flags & FDIV_DIV) {
	if (flags & FDIV_MOD) { /* return both div and mod */
	    res =  kcons(K, tv_div, kcons(K, tv_mod, KNIL));
	} else {
	    res = tv_div;
	}
    } else {
	res = tv_mod;
    }
    kapply_cc(K, res);
}

/* 12.5.10 positive?, negative? */
/* use ftyped_predp */

/* 12.5.11 odd?, even? */
/* use ftyped_predp */

/* Helpers for positive?, negative?, odd? & even? */
bool kpositivep(TValue n) { return ivalue(n) > 0; }
bool knegativep(TValue n) { return ivalue(n) < 0; }
bool koddp(TValue n) { return (ivalue(n) & 1) != 0; }
bool kevenp(TValue n) { return (ivalue(n) & 1) == 0; }

/* 12.5.12 abs */
void kabs(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);

    bind_1tp(K, "abs", ptree, "number", knumberp, n);

    switch(ttype(n)) {
    case K_TFIXINT: {
	int32_t i = ivalue(n);
	kapply_cc(K, i < 0? i2tv(-i) : n);
    }
    case K_TEINF:
	kapply_cc(K, KEPINF);
    default:
	/* shouldn't happen */
	assert(0);
	return;
    }
}

/* 12.5.13 min, max */
/* NOTE: this does two passes, one for error checking and one for doing
 the actual work */
void kmin_max(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    /*
    ** xparams[0]: symbol name
    ** xparams[1]: bool: true min, false max
    */
    UNUSED(denv);
    
    char *name = ksymbol_buf(xparams[0]);
    bool minp = bvalue(xparams[1]);

    /* cycles are allowed, loop counting pairs */
    int32_t dummy; /* don't care about count of cycle pairs */
    int32_t pairs = check_typed_list(K, name, "number", knumberp, true, ptree,
	&dummy);
    
    TValue res;
    bool one_finite = false;
    TValue break_val;
    if (minp) {
	res = KEPINF;
	break_val = KEMINF; /* min possible number */
    } else {
	res = KEMINF;
	break_val = KEPINF; /* max possible number */
    }

    TValue tail = ptree;
    while(pairs--) {
	TValue first = kcar(tail);
	tail = kcdr(tail);

	if (ttiseinf(first)) {
	    if (tv_equal(first, break_val)) {
		res = first;
		break;
	    }
	} else if (!one_finite) {
	    res = first;
	    one_finite = true;
	} else if (minp) {
	    if (ivalue(first) < ivalue(res))
		res = first;
	} else { /* maxp */
	    if (ivalue(first) > ivalue(res))
		res = first;
	}
    }
    kapply_cc(K, res);
}

/* 12.5.14 gcm, lcm */

/* gcd for two numbers */
int32_t gcd2(int32_t a, int32_t b)
{
    /* work with positive numbers */
    if (a < 0)
	a = -a;
    if (b < 0)
	b = -b;

    /* this is a vanilla binary gcd algorithm */ 
    int32_t powerof2;

    /* the easy cases first, unlike the general kernel gcd the
     gcd2 of a number and zero is zero */
    if (a == 0)
	return b;
    else if (b == 0)
	return a;
 
    for (powerof2 = 0; ((a & 1) == 0) && 
	     ((b & 1) == 0); ++powerof2, a >>= 1, b >>= 1)
	;
 
    while(a != 0 && b!= 0) {
	/* either a or b are odd, make them both odd */
	for (; (a & 1) == 0; a >>= 1)
	    ;
	for (; (b & 1) == 0; b >>= 1)
	    ;

	/* now the difference is sure to be even */
	if (a < b) {
	    b = (b - a) >> 1;
	} else {
	    a = (a - b) >> 1;
	}
    }
 
    return (a == 0? b : a) << powerof2;
}

void kgcd(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    /* cycles are allowed, loop counting pairs */
    int32_t dummy; /* don't care about count of cycle pairs */
    int32_t pairs = check_typed_list(K, "gcd", "number", knumberp, true,
				     ptree, &dummy);

    TValue res;

    if (pairs) {
	TValue tail = ptree;
	bool seen_zero = false; 
	bool seen_finite_non_zero = false; 
	int32_t finite_gcd = 0;

	while(pairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);
	    if (kfast_zerop(first)) {
		seen_zero = true;
	    } else if (ttisfixint(first)) {
		seen_finite_non_zero = true;
		finite_gcd = gcd2(finite_gcd, ivalue(first));
	    }
	}
	if (seen_finite_non_zero) {
	    res = i2tv(finite_gcd);
	} else if (seen_zero) {
           /* report */
	    klispE_throw(K, "gcd: result has no primary value");
	} else {
	    res = KEPINF; /* report */
	}
    } else {
	res = KEPINF; /* report: (gcd) = #e+infinity */
    }

    kapply_cc(K, res);
}

void klcm(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    /* cycles are allowed, loop counting pairs */
    int32_t dummy; /* don't care about count of cycle pairs */
    int32_t pairs = check_typed_list(K, "lcm", "number", knumberp, true, 
				     ptree, &dummy);
    /* we will need to loop again after obtaining the gcd */
    int32_t saved_pairs = pairs; 
				 
    TValue res = i2tv(1); /* report: (lcm) = 1 */
    /* lcm is +infinite if there is any infinite number, must still loop 
       to check for zero but returns #e+infinty */
    bool seen_infinite = false; 
    int32_t finite_gcd = 0;
    
    TValue tail = ptree;
    while(pairs--) {
	TValue first = kcar(tail);
	tail = kcdr(tail);
	if (ttiseinf(first)) {
	    seen_infinite = true;
	    res = KEPINF; /* report */
	} else if (kfast_zerop(first)) {
	    klispE_throw(K, "lcm: result has no primary");
	    return;
	} else if (!seen_infinite) {
	    finite_gcd = gcd2(finite_gcd, ivalue(first));
	}
    }

    if (!seen_infinite && saved_pairs) {
	/* now collect the lcm proper, there are no zero and no infinities,
	   finite_gcd can't be zero because there are at least one finite,
	   non-zero number */
	tail = ptree;
	pairs = saved_pairs;
	int32_t lcm = 1;
	while(pairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);
	    int32_t first_i = ivalue(first);
            /* no remainder */
	    lcm *= (first_i < 0? -first_i : first_i) / finite_gcd; 
	}
	res = i2tv(lcm);
    }
    kapply_cc(K, res);
}
