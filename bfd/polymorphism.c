#include "polymorphism.h"
#include <assert.h>                             // assert
#include <stddef.h>                             // NULL, size_t
#include <stdlib.h>                             // abort
#include <string.h>                             // strcmp, strlen
#include <stdio.h>                              // printf

extern char *cplus_demangle_v3 (const char *mangled, int options);   // defined in libiberty/cp-demangle.c
extern void *xmalloc(size_t);
extern char *xstrdup(const char*);

static void abortwhy(char const *const p)
{
    printf("\n========== abort() called - '%s' ==========\n", p);
    abort();
}

struct MappingTypeinfoVtable {
    size_t offset_typeinfo;
    size_t offset_vtable;
};

static struct MappingTypeinfoVtable *g_polymap = NULL;
static size_t g_polymap_size = 0u;

struct ListSymbolNode {
    struct ListSymbolNode *next;
    char *name;
    size_t offset;
};

struct ListSymbol {
    struct ListSymbolNode *head, *tail;
};

struct ListSymbol g_vtables = {NULL,NULL}, g_typeinfos = {NULL,NULL};

static struct ListSymbolNode *NewListSymbolNode(char const *const arg)
{
    struct ListSymbolNode *const p = xmalloc( sizeof(struct ListSymbolNode) );
    p->next   = NULL;
    p->name   = xstrdup(arg);
    p->offset = 0u;
    return p;
}

static struct ListSymbolNode *ListSymbol_find(struct ListSymbol *const ls, char const *const arg)
{
    if ( NULL==ls || NULL==arg || '\0'==arg[0u] ) return NULL;

    for ( struct ListSymbolNode *n = ls->head; NULL != n; n = n->next )
    {
       if ( 0 == strcmp(n->name,arg) ) return n;
    }

    return NULL;
}

static void ListSymbol_append(struct ListSymbol *const ls, char const *const arg)
{
   if ( NULL==ls || NULL==arg || '\0'==arg[0u] ) return;

   if ( NULL != ListSymbol_find(ls,arg) ) return;

   struct ListSymbolNode *const newtail = NewListSymbolNode(arg);

   if ( NULL == ls->head )
   {
       ls->head = ls->tail = newtail;
       return;
   }

   ls->tail->next = newtail;
   ls->tail       = newtail;
}

static int ListSymbol_revise(struct ListSymbol *const ls, char const *const arg, size_t const offset)
{
    if ( NULL==ls || NULL==arg || '\0'==arg[0u] ) return 0;

    struct ListSymbolNode *const p = ListSymbol_find(ls,arg);

    if ( NULL == p ) return 0;

    p->offset = offset;

    return 1;
}

static size_t Count_Pairs(void)
{
    size_t count = 0u;

    for ( struct ListSymbolNode const *n = g_vtables.head; NULL != n; n = n->next )
    {
      if ( NULL != ListSymbol_find(&g_typeinfos,n->name) ) ++count;
    }

    return count;
}

int Polymorphism_Process_Symbol_1st_Run(char const *const arg)
{
    if ( NULL == arg ) return 0;

    if ( strlen(arg) < 6u ) return 0;

    /**/ if ( 0 == strncmp("_ZTV", arg, 4u) ) ListSymbol_append( &g_vtables  , arg + 4u );
    else if ( 0 == strncmp("_ZTI", arg, 4u) ) ListSymbol_append( &g_typeinfos, arg + 4u );
    else return 0;

    return 1;
}

int Polymorphism_Process_Symbol_2nd_Run(char const *const arg, size_t const offset)
{
    if ( NULL == arg ) return 0;

    if ( strlen(arg) < 6u ) return 0;

    /**/ if ( 0 == strncmp("_ZTV", arg, 4u) ) { if ( !ListSymbol_revise( &g_vtables  , arg + 4u, offset ) ) abortwhy("uknown symbol"); }
    else if ( 0 == strncmp("_ZTI", arg, 4u) ) { if ( !ListSymbol_revise( &g_typeinfos, arg + 4u, offset ) ) abortwhy("uknown symbol"); }
    else return 0;

    return 1;
}

static void Polymorphism_Populate_Map_Numbers(void)
{
    if ( NULL == g_vtables.head )
    {
      puts("list of vtables is empty");
      return;
    }

    printf("std::pair< std::size_t, std::size_t > map_typeinfo_vtable[] = {\n");

    size_t index = -1;

    for ( struct ListSymbolNode const *nv = g_vtables.head; NULL != nv; nv = nv->next )
    {
      struct ListSymbolNode const *const ni = ListSymbol_find(&g_typeinfos,nv->name);
      if ( NULL == ni ) continue;
      ++index;
      g_polymap[index].offset_typeinfo = ni->offset;
      g_polymap[index].offset_vtable   = nv->offset;
      printf("    { %*zu, %*td },  // _Z%s\n", 20, g_polymap[index].offset_typeinfo, 20, g_polymap[index].offset_vtable, nv->name);
    }
    puts("};");
}

size_t Polymorphism_Finalise_Symbols_And_Create_Map(void **const pp)
{
    *pp = NULL;
    g_polymap_size = Count_Pairs();
    if ( 0u == g_polymap_size ) return 0u;
    size_t const size_in_bytes = g_polymap_size * sizeof(struct MappingTypeinfoVtable);
    g_polymap = xmalloc(size_in_bytes);
    Polymorphism_Populate_Map_Numbers();

    for ( struct ListSymbol *ls = &g_vtables; ; ls = &g_typeinfos )
    {
        for ( struct ListSymbolNode *n = ls->head; NULL != n; )
        {
            free(n->name);
            void *const to_be_freed = n;
            n = n->next;
            free(to_be_freed);
        }
        if ( &g_typeinfos == ls ) break;
    }

    g_vtables.head = g_vtables.tail = g_typeinfos.head = g_typeinfos.tail = NULL;

    *pp = g_polymap;
    return size_in_bytes;
}

//================================================================================================
//================================================================================================
//================================================================================================
#if 0  // This code is crashing -- don't know why
      char *const demangled = cplus_demangle_v3(tmp,0); //DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE);
      if ( NULL != demangled )
      {
          printf(" - %s", demangled);
          if ( tmp != demangled )
          {
              //printf(" - about to free demangled - ");
              free(demangled);
              //printf(" - demangled freed - ");
          }
      }
      //printf(" - about to free tmp - ");

      //printf(" - tmp freed - ");
#endif
