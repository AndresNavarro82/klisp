/*
** kgpair_mut.c
** Pair mutation features for the ground environment
** See Copyright Notice in klisp.h
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "kstate.h"
#include "kobject.h"
#include "kpair.h"
#include "kcontinuation.h"
#include "ksymbol.h"
#include "kerror.h"

#include "kghelpers.h"
#include "kgpair_mut.h"
#include "kgeqp.h" /* eq? checking in memq and assq */

/* 4.7.1 set-car!, set-cdr! */
void set_carB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2tp(K, "set-car!", ptree, "pair", ttispair, pair, 
	     "any", anytype, new_car);

    if(!kis_mutable(pair)) {
	    klispE_throw(K, "set-car!: immutable pair");
	    return;
    }
    kset_car(pair, new_car);
    kapply_cc(K, KINERT);
}

void set_cdrB(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    (void) denv;
    (void) xparams;
    bind_2tp(K, "set-cdr!", ptree, "pair", ttispair, pair, 
	     "any", anytype, new_cdr);
    
    if(!kis_mutable(pair)) {
	    klispE_throw(K, "set-cdr!: immutable pair");
	    return;
    }
    kset_cdr(pair, new_cdr);
    kapply_cc(K, KINERT);
}

/* Helper for copy-es-immutable & copy-es */
void copy_es(klisp_State *K, TValue *xparams, 
	     TValue ptree, TValue denv)
{
    /*
    ** xparams[0]: copy-es-immutable symbol
    ** xparams[1]: boolean (#t: use mutable pairs, #f: use immutable pairs)
    */
    char *name = ksymbol_buf(xparams[0]);
    bool mut_flag = bvalue(xparams[1]);
    bind_1p(K, name, ptree, obj);

    TValue copy = copy_es_immutable_h(K, name, obj, mut_flag);
    kapply_cc(K, copy);
}

/* 4.7.2 copy-es-immutable */
/* uses copy_es */

/*
** This is in a helper method to use it from $lambda, $vau, etc
**
** We mark each seen mutable pair with the corresponding copied 
** immutable pair to construct a structure that is isomorphic to 
** the original.
** All objects that aren't mutable pairs are retained without 
** copying
** sstack is used to keep track of pairs and tbstack is used
** to keep track of which of car or cdr we were copying,
** 0 means just pushed, 1 means return from car, 2 means return from cdr
*/
TValue copy_es_immutable_h(klisp_State *K, char *name, TValue obj, 
			   bool mut_flag)
{
    /* 
    ** GC: obj is rooted because it is in the stack at all times.
    ** The copied pair should be kept safe some other way
    */
    TValue copy = obj;

    assert(ks_sisempty(K));
    assert(ks_tbisempty(K));

    ks_spush(K, obj);
    ks_tbpush(K, ST_PUSH);

    while(!ks_sisempty(K)) {
	char state = ks_tbpop(K);
	TValue top = ks_spop(K);

	if (state == ST_PUSH) {
	    /* if the pair is immutable & we are constructing immutable
	       pairs there is no need to copy */
	    if (ttispair(top) && (mut_flag || kis_mutable(top))) {
		if (kis_marked(top)) {
		    /* this pair was already seen, use the same */
		    copy = kget_mark(top);
		} else {
		    TValue new_pair = kcons_g(K, mut_flag, KINERT, KINERT);
		    kset_mark(top, new_pair);
		    /* leave the pair in the stack, continue with the car */
		    ks_spush(K, top);
		    ks_tbpush(K, ST_CAR);
		    
		    ks_spush(K, kcar(top));
		    ks_tbpush(K, ST_PUSH);
		}
	    } else {
		copy = top;
	    }
	} else { /* last action was a pop */
	    TValue new_pair = kget_mark(top);
	    if (state == ST_CAR) {
		kset_car(new_pair, copy);
		/* leave the pair on the stack, continue with the cdr */
		ks_spush(K, top);
		ks_tbpush(K, ST_CDR);

		ks_spush(K, kcdr(top));
		ks_tbpush(K, ST_PUSH);
	    } else {
		kset_cdr(new_pair, copy);
		copy = new_pair;
	    }
	}
    }
    unmark_tree(K, obj);
    return copy;
}

/* 5.8.1 encycle! */
void encycleB(klisp_State *K, TValue *xparams, TValue ptree, 
		      TValue denv)
{
/* ASK John: can the object be a cyclic list of length less than k1+k2? 
   the wording of the report seems to indicate that can't be the case, 
   and here it makes sense to forbid it because otherwise the list-metrics 
   of the result would differ with the expected ones (cf list-tail). 
   So here an error is signaled if the improper list cyclic with less pairs
   than needed */
    (void) denv;
    (void) xparams;
    /* XXX: should be integer instead of fixint, but that's all
       we have for now */
    bind_3tp(K, "encycle!", ptree, "any", anytype, obj,
	     "finite integer", ttisfixint, tk1,
	     "finite integer", ttisfixint, tk2);

    int32_t k1 = ivalue(tk1);
    int32_t k2 = ivalue(tk2);

    if (k1 < 0 || k2 < 0) {
	klispE_throw(K, "encycle!: negative index");
	return;
    }

    TValue tail = obj;

    while(k1) {
	if (!ttispair(tail)) {
	    unmark_list(K, obj);
	    klispE_throw(K, "encycle!: non pair found while traversing "
			 "object");
	    return;
	} else if (kis_marked(tail)) {
	    unmark_list(K, obj);
	    klispE_throw(K, "encycle!: too few pairs in cyclic list");
	    return;
	}
	kmark(tail);
	tail = kcdr(tail);
	--k1;
    }

    TValue fcp = tail;

    /* if k2 == 0 do nothing (but this still checks that the obj
       has at least k1 pairs */
    if (k2 != 0) {
	--k2; /* to have cycle length k2 we should discard k2-1 pairs */
	while(k2) {
	    if (!ttispair(tail)) {
		unmark_list(K, obj);
		klispE_throw(K, "encycle!: non pair found while traversing "
			     "object");
		return;
	    } else if (kis_marked(tail)) {
		unmark_list(K, obj);
		klispE_throw(K, "encycle!: too few pairs in cyclic list");
		return;
	    }
	    kmark(tail);
	    tail = kcdr(tail);
	    --k2;
	}
	if (!kis_mutable(tail)) {
	    unmark_list(K, obj);
	    klispE_throw(K, "encycle!: immutable pair");
	    return;
	} else {
	    kset_cdr(tail, fcp);
	}
    }
    unmark_list(K, obj);
    kapply_cc(K, KINERT);
}

/* Helpers for append! */


inline void appendB_clear_last_pairs(klisp_State *K, TValue ls)
{
    UNUSED(K);
    while(ttispair(ls) && kis_marked(ls)) {
	TValue first = ls;
	ls = kget_mark(ls);
	kunmark(first);
    }
}

/* Check that all lists (except last) are acyclic lists with non repeated mutable 
   last pair (if not nil), return a list of objects so that the cdr of the odd
   objects (1 based) should be set to the next object in the list (this will
   encycle! the result if necessary) */
TValue appendB_get_lss_endpoints(klisp_State *K, TValue lss, int32_t apairs, 
				 int32_t cpairs)
{
    TValue dummy = kcons(K, KINERT, KNIL);
    TValue last_pair = dummy;
    TValue tail = lss;
    /* this is a list of last pairs using the marks to link the pairs) */
    TValue last_pairs = KNIL;
    TValue last_apair;

    while(apairs != 0 || cpairs != 0) {
	int32_t pairs;
	
	if (apairs == 0) {
	    /* this is the first run of the loop (if there is no acyclic part) 
	       or the second run of the loop (the cyclic part), 
	       must remember the last acyclic pair to encycle! the result */
	    last_apair = last_pair;
	    pairs = cpairs;
	} else {
	    /* this is the first (maybe only) run of the loop 
	       (the acyclic part) */
	    pairs = apairs;
	}

	while(pairs--) {
	    TValue first = kcar(tail);
	    tail = kcdr(tail);

	    /* skip over nils */
	    if (ttisnil(first))
		continue; 

	    TValue ftail = first;
	    TValue flastp = first;

	    /* find the last pair to check the object */
	    while(ttispair(ftail) && !kis_marked(ftail)) {
		kmark(ftail);
		flastp = ftail; /* remember last pair */
		ftail = kcdr(ftail);
	    }
	
	    /* can't unmark the list till the errors are checked,
	       otherwise the unmarking may be incorrect */
	    if (ttisnil(tail)) {
		/* last argument has special treatment */
		if (ttispair(ftail) && ttisnil(kcdr(ftail))) {
		    /* repeated last pair, this is the only check
		       that is done on the last argument */
		    appendB_clear_last_pairs(K, last_pairs);
		    unmark_list(K, first);
		    klispE_throw(K, "append!: repeated last pairs");
		    return KINERT;
		} else {
		    unmark_list(K, first);
		    /* add last object to the endpoints list, don't add
		       its last pair */
		    kset_cdr(last_pair, kcons(K, first, KNIL));
		}
	    } else { /* non final argument, must be an acyclic list 
			with unique, mutable last pair */
		if (ttisnil(ftail)) {
		    /* acyclic list with non repeated last pair,
		       check mutability */
		    unmark_list(K, first);
		    if (kis_immutable(flastp)) {
			appendB_clear_last_pairs(K, last_pairs);
			klispE_throw(K, "append!: immutable pair found");
		    }
		    /* add the last pair to the list of last pairs */
		    kset_mark(flastp, last_pairs);
		    last_pairs = flastp;
		
		    /* add both the first and last pair to the endpoints 
		       list */
		    TValue new_pair = kcons(K, first, KNIL);
		    kset_cdr(last_pair, new_pair);
		    last_pair = new_pair;
		    new_pair = kcons(K, flastp, KNIL);
		    kset_cdr(last_pair, new_pair);
		    last_pair = new_pair;
		} else {
		    /* impoper list or repeated last pair or cyclic list */
		    appendB_clear_last_pairs(K, last_pairs);
		    unmark_list(K, first);

		    if (ttispair(ftail)) {
			if (ttisnil(kcdr(ftail))) {
			    klispE_throw(K, "append!: repeated last pairs");
			} else {
			    klispE_throw(K, "append!: cyclic list as non last "
					 "argument");
			}  
		    } else {
			klispE_throw(K, "append!: improper list as non last "
				     "argument");
		    }
		    return KINERT;
		}
	    }
	}
	if (apairs != 0) {
	    /* acyclic part done */
	    apairs = 0;
	} else {
	    /* cyclic part done, program encycle if necessary */
	    cpairs = 0;
	    if (!tv_equal(last_apair, last_pair)) {
		TValue first_cpair = kcadr(last_apair);
		kset_cdr(last_pair, kcons(K, first_cpair, KNIL));
	    } else {
		/* all elements of the cycle are (), add extra
		   nil to simplify the code setting the cdrs */
		kset_cdr(last_pair, kcons(K, KNIL, KNIL));
	    }
	}
    }

    appendB_clear_last_pairs(K, last_pairs);

    /* discard the first element (there is always one) because it
     isn't necessary, the list is used to set the last pairs of
     the objects to the correspoding next first pair */
    return kcddr(dummy);
}

/* 6.4.1 append! */
void appendB(klisp_State *K, TValue *xparams, TValue ptree, 
	      TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);
    if (ttisnil(ptree)) {
	klispE_throw(K, "append!: no lists");
	return;
    } else if (!ttispair(ptree)) {
	klispE_throw(K, "append!: bad ptree");
	return;
    } else if (ttisnil(kcar(ptree))) {
	klispE_throw(K, "append!: empty first list");
	return;
    }
    TValue lss = ptree;
    TValue first_ls = kcar(lss);
    int32_t cpairs;
    /* ASK John: if encycle! has only one argument, can't it be cyclic? 
     the report says no, but the wording is poor */
    int32_t pairs = check_list(K, "append!", false, first_ls, &cpairs);
    
    pairs = check_list(K, "append!", true, lss, &cpairs);
    int32_t apairs = pairs - cpairs;

    TValue endpoints = 
	appendB_get_lss_endpoints(K, lss, apairs, cpairs);
    /* connect all the last pairs to the corresponding next first pair,
     endpoints is even */
    while(!ttisnil(endpoints)) {
	TValue first = kcar(endpoints);
	endpoints = kcdr(endpoints);
	TValue second = kcar(endpoints);
	endpoints = kcdr(endpoints);
	kset_cdr(first, second);
    }
    kapply_cc(K, KINERT);
}

/* 6.4.2 copy-es */
/* uses copy_es helper (above copy-es-immutable) */

/* 6.4.3 assq */
/* REFACTOR: do just one pass, maybe use generalized accum function */
void assq(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);

    bind_2p(K, "assq", ptree, obj, ls);
    /* first pass, check structure */
    int32_t dummy;
    int32_t pairs = check_typed_list(K, "assq", "pair", kpairp,
				     true, ls, &dummy);
    TValue tail = ls;
    TValue res = KNIL;
    while(pairs--) {
	TValue first = kcar(tail);
	if (eq2p(K, kcar(first), obj)) {
	    res = first;
	    break;
	}
	tail = kcdr(tail);
    }

    kapply_cc(K, res);
}

/* 6.4.3 memq? */
/* REFACTOR: do just one pass, maybe use generalized accum function */
void memqp(klisp_State *K, TValue *xparams, TValue ptree, TValue denv)
{
    UNUSED(xparams);
    UNUSED(denv);

    bind_2p(K, "memq?", ptree, obj, ls);
    /* first pass, check structure */
    int32_t dummy;
    int32_t pairs = check_list(K, "memq?", true, ls, &dummy);
    TValue tail = ls;
    TValue res = KFALSE;
    while(pairs--) {
	TValue first = kcar(tail);
	if (eq2p(K, first, obj)) {
	    res = KTRUE;
	    break;
	}
	tail = kcdr(tail);
    }

    kapply_cc(K, res);
}
