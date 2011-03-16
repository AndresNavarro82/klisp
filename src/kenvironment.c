/*
** kenvironment.c
** Kernel Environments
** See Copyright Notice in klisp.h
*/

#include <string.h>

#include "kenvironment.h"
#include "kpair.h"
#include "ksymbol.h"
#include "kobject.h"
#include "kerror.h"
#include "kstate.h"
#include "kmem.h"

/* keyed dynamic vars */
#define env_keyed_parents(env_) (tv2env(env_)->keyed_parents)
#define env_keyed_node(env_) (tv2env(env_)->keyed_node)
#define env_keyed_key(env_) (kcar(env_keyed_node(env_)))
#define env_keyed_val(env_) (kcdr(env_keyed_node(env_)))
#define env_is_keyed(env_) (!ttisnil(env_keyed_node(env_)))
/* env_ should be keyed! */
#define env_has_key(env_, k_) (tv_equal(env_keyed_key(env_), (k_)))
/* TEMP: for now allow only a single parent */
TValue kmake_environment(klisp_State *K, TValue parents)
{
    Environment *new_env = klispM_new(K, Environment);

    /* header + gc_fields */
    new_env->next = K->root_gc;
    K->root_gc = (GCObject *) new_env;
    new_env->gct = 0;
    new_env->tt = K_TENVIRONMENT;
    new_env->flags = 0;

    /* environment specific fields */
    new_env->mark = KFALSE;    
    new_env->parents = parents;
    /* TEMP: for now the bindings are an alist */
    new_env->bindings = KNIL;
    /* TEMP: this could be passed in by the contructor */
    /* Contruct the list of keyed parents */
    /* MAYBE: this could be optimized to avoid repetition of parents */
    TValue kparents;
    if (ttisnil(parents)) {
	kparents = KNIL;
    } else if (ttisenvironment(parents)) {
	kparents = env_is_keyed(parents)? parents : env_keyed_parents(parents);
    } else {
	/* list of parents, for now, just append them */
	/* GC: root intermediate objs */
	TValue dummy = kcons(K, KNIL, KNIL);
	TValue tail = dummy;
	while(!ttisnil(parents)) {
	    TValue parent = kcar(parents);
	    TValue pkparents = env_keyed_parents(parent);
	    while(!ttisnil(pkparents)) {
		TValue next;
		if (ttisenvironment(pkparents)) {
		    next = pkparents;
		    pkparents = KNIL;
		} else {
		    next = kcar(pkparents);
		    pkparents = kcdr(pkparents);
		}
		TValue new_pair = kcons(K, next, KNIL);
		kset_cdr(tail, new_pair);
		tail = new_pair;
	    }
	    parents = kcdr(parents);
	}
	kparents = kcdr(dummy);
	/* if it's just one env switch from (env) to env. */
	if (ttispair(kparents) && ttisnil(kcdr(kparents)))
	    kparents = kcar(kparents);
    }
    new_env->keyed_parents = kparents;
    new_env->keyed_node = KNIL;

    return gc2env(new_env);
}

/* 
** Helper function for kadd_binding and kget_binding,
** returns KNIL or a pair with sym as car.
*/
TValue kfind_local_binding(klisp_State *K, TValue bindings, TValue sym)
{
    (void) K;
    while(!ttisnil(bindings)) {
	TValue first = kcar(bindings);
	TValue first_sym = kcar(first);
	if (tv_equal(sym, first_sym))
	    return first;
	bindings = kcdr(bindings);
    }
    return KNIL;
}

/*
** Some helper macros
*/
#define kenv_parents(kst_, env_) (tv2env(env_)->parents)
#define kenv_bindings(kst_, env_) (tv2env(env_)->bindings)

void kadd_binding(klisp_State *K, TValue env, TValue sym, TValue val)
{
    TValue oldb = kfind_local_binding(K, kenv_bindings(K, env), sym);

    if (ttisnil(oldb)) {
	/* XXX: unrooted pair */
	TValue new_pair = kcons(K, sym, val);
	kenv_bindings(K, env) = kcons(K, new_pair, kenv_bindings(K, env));
    } else {
	kset_cdr(oldb, val);
    }
}

/* This works no matter if parents is a list or a single environment */
inline bool try_get_binding(klisp_State *K, TValue env, TValue sym, 
			    TValue *value)
{
    /* assume the stack may be in use, keep track of pushed objs */
    int pushed = 1;
    ks_spush(K, env);

    while(pushed) {
	TValue obj = ks_spop(K);
	--pushed;
	if (ttisnil(obj)) {
	    continue;
	} else if (ttisenvironment(obj)) {
	    TValue oldb = kfind_local_binding(K, kenv_bindings(K, obj), sym);
	    if (!ttisnil(oldb)) {
		/* remember to leave the stack as it was */
		ks_sdiscardn(K, pushed);
		*value = kcdr(oldb);
		return true;
	    }
	    TValue parents = kenv_parents(K, obj);
	    ks_spush(K, parents);
	    ++pushed;
	} else { /* parent list */
	    ks_spush(K, kcdr(obj));
	    ks_spush(K, kcar(obj));
	    pushed += 2;
	}
    }
    *value = KINERT;
    return false;
}

TValue kget_binding(klisp_State *K, TValue env, TValue sym)
{
    TValue value;
    if (try_get_binding(K, env, sym, &value)) {
	return value;
    } else {
	klispE_throw_extra(K, "Unbound symbol: ", ksymbol_buf(sym));
	/* avoid warning */
	return KINERT;
    }
}

bool kbinds(klisp_State *K, TValue env, TValue sym)
{
    TValue value;
    return try_get_binding(K, env, sym, &value);
}

/* keyed dynamic vars */

/* MAYBE: This could be combined with the default constructor */
TValue kmake_keyed_static_env(klisp_State *K, TValue parent, TValue key, 
			      TValue val)
{
    TValue new_env = kmake_environment(K, parent);
    env_keyed_node(new_env) = kcons(K, key, val);
    return new_env;
}

inline bool try_get_keyed(klisp_State *K, TValue env, TValue key, 
			  TValue *value)
{
    /* MAYBE: this could be optimized to mark environments to avoid
     repetition */
    /* assume the stack may be in use, keep track of pushed objs */
    int pushed = 1;
    ks_spush(K, env);

    while(pushed) {
	TValue obj = ks_spop(K);
	--pushed;
	if (ttisnil(obj)) {
	    continue;
	} else if (ttisenvironment(obj)) {
	    /* obj is guaranteed to be a keyed env */
	    if (env_has_key(obj, key)) {
		/* remember to leave the stack as it was */
		ks_sdiscardn(K, pushed);
		*value = env_keyed_val(obj);
		return true;
	    } else {
		TValue parents = kenv_parents(K, obj);
		ks_spush(K, parents);
		++pushed;
	    }
	} else { /* parent list */
	    ks_spush(K, kcdr(obj));
	    ks_spush(K, kcar(obj));
	    pushed += 2;
	}
    }
    *value = KINERT;
    return false;
}

TValue kget_keyed_static_var(klisp_State *K, TValue env, TValue key)
{
    TValue value;
    if (try_get_keyed(K, env, key, &value)) {
	return value;
    } else {
	klispE_throw(K, "keyed-static-get: Unbound keyed static variable");
	/* avoid warning */
	return KINERT;
    }
}
