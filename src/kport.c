/*
** kport.c
** Kernel Ports
** See Copyright Notice in klisp.h
*/

#include <stdio.h>
#include <assert.h>

#include "kport.h"
#include "kobject.h"
#include "kstate.h"
#include "kmem.h"
#include "kerror.h"
#include "kstring.h"

TValue kmake_port(klisp_State *K, TValue filename, bool writep, TValue name, 
			  TValue si)
{
    /* for now always use text mode */
    FILE *f = fopen(kstring_buf(filename), writep? "w": "r");
    if (f == NULL) {
	klispE_throw(K, "Create port: could't open file");
	return KINERT;
    } else {
	return kmake_std_port(K, filename, writep, name, si, f);
    }
}

/* this is for creating ports for stdin/stdout/stderr &
 also a helper for the above */
TValue kmake_std_port(klisp_State *K, TValue filename, bool writep, 
		      TValue name, TValue si, FILE *file)
{
    Port *new_port = klispM_new(K, Port);

    /* header + gc_fields */
    new_port->next = K->root_gc;
    K->root_gc = (GCObject *)new_port;
    new_port->gct = 0;
    new_port->tt = K_TPORT;
    new_port->flags = writep? K_FLAG_OUTPUT_PORT : K_FLAG_INPUT_PORT;

    /* portinuation specific fields */
    new_port->name = name;
    new_port->si = si;
    new_port->filename = filename;
    new_port->file = file;

    return gc2port(new_port);
}

/* if the port is already closed do nothing */
void kclose_port(klisp_State *K, TValue port)
{
    assert(ttisport(port));

    if (!kport_is_closed(port)) {
	FILE *f = tv2port(port)->file;
	if (f != stdin && f != stderr && f != stdout)
	    fclose(f); /* it isn't necessary to check the close ret val */

	kport_set_closed(port);
    }

    return;
}