#include "polymorphism.h"
#include <assert.h>                             // assert
#include <stddef.h>                             // NULL, size_t
#include <stdlib.h>                             // abort
#include <string.h>                             // memset, strlen, strcpy
#include <stdio.h>                              // printf

extern char *cplus_demangle_v3 (const char *mangled, int options);   // defined in libiberty/cp-demangle.c

static void abortwhy(char const *const p)
{
    printf("\n========== %s ==========\n", p);
    abort();
}

#define BYTES_PER_ALLOC (16384u)
#define BUFSIZNODE (BYTES_PER_ALLOC - sizeof(void*) - 1u)
#define TERM ('\r')

struct NodeOffset {
   void *next;
   size_t count;
   ptrdiff_t offsets[ BYTES_PER_ALLOC / sizeof(ptrdiff_t) - (sizeof(void*)/sizeof(ptrdiff_t)) - !!(sizeof(void*)%sizeof(ptrdiff_t)) - 1u ];
};

struct ListOffset {
    struct NodeOffset *head, *tail;
};

struct ListOffset g_vtables_offsets = {NULL,NULL}, g_typeinfos_offsets = {NULL,NULL};

struct Node {
   void *next;
   char buf[BUFSIZNODE];
   char terminator;
};

typedef char my_static_assert0[ (BYTES_PER_ALLOC == sizeof(struct Node)) ? 1 : -1 ];

struct List {
    struct Node *head, *tail;
};

static struct List g_vtables = {NULL,NULL}, g_typeinfos = {NULL,NULL};

static void AppendOffset(struct List const *const p, ptrdiff_t const offset)
{
    struct ListOffset *plo;
    /**/ if ( p == &g_vtables   ) plo = &g_vtables_offsets  ;
    else if ( p == &g_typeinfos ) plo = &g_typeinfos_offsets;
    else abortwhy("Dodgy pointer given to AppendOffset");

    if ( NULL == plo->head )
    {
        plo->head = plo->tail = malloc( sizeof(struct NodeOffset) );
        if ( NULL == plo->head ) abortwhy("Failed to allocate head Offset node");
        plo->head->next = NULL;
        memset(plo->head->offsets, 0, sizeof plo->head->offsets);
        plo->head->count = 0u;
    }

    if ( plo->tail->count < (sizeof plo->tail->offsets / sizeof *plo->tail->offsets) )
    {
        plo->tail->offsets[ plo->tail->count++ ] = offset;
        return;
    }

    struct NodeOffset *const mynewnode = malloc( sizeof(struct NodeOffset) );
    if ( NULL == mynewnode ) abortwhy("Failed to allocate next Offset node");
    mynewnode->next = NULL;
    memset(mynewnode->offsets, 0, sizeof mynewnode->offsets);
    mynewnode->offsets[0u] = offset;
    mynewnode->count = 1u;
    plo->tail->next = mynewnode;
    plo->tail = mynewnode;
}

static ptrdiff_t GetOffset(struct List const *const p, size_t const index)
{
    struct ListOffset *plo;
    /**/ if ( p == &g_vtables   ) plo = &g_vtables_offsets  ;
    else if ( p == &g_typeinfos ) plo = &g_typeinfos_offsets;
    else abortwhy("Dodgy pointer given to AppendOffset");

    size_t n = index / (sizeof plo->tail->offsets / sizeof *plo->tail->offsets),
           m = index % (sizeof plo->tail->offsets / sizeof *plo->tail->offsets);

    struct NodeOffset const *pno = plo->head;

    while ( n-- )
    {
        pno = pno->next;
        if ( NULL == pno ) abortwhy("Invalid index given to GetOffset");
    }

    return pno->offsets[m];
}

static void Increment_Past_Null(char const **const pp)
{
    while ( '\0' != *(*pp)++ );
}

static struct Node *NewNode(char const *const arg)
{
   struct Node *const p = malloc( sizeof(struct Node) );
   if ( NULL == p ) return NULL;
   p->next = NULL;
   memset(p->buf, TERM, sizeof p->buf);
   p->terminator = TERM;
   if ( strlen(arg) >= BUFSIZNODE ) abortwhy("Length of symbol is longer than (BUFSIZE-1)");
   strcpy(p->buf, arg);   // strlen(arg) must be < BUFSIZNODE
   return p;
}

static void List_append(struct List *const ll, char const *const arg, size_t const offset)
{
   if ( NULL==ll || NULL==arg ) return;
   size_t const arglen = strlen(arg);
   if ( (0u == arglen) || (arglen >= BUFSIZNODE) ) return;

   AppendOffset(ll,offset);

   if ( NULL == ll->head )
   {
       printf("--- Creating the head\n");
       ll->head = ll->tail = NewNode(arg);
       if ( NULL == ll->head ) abortwhy("Failed to allocate head node");
       return;
   }

   size_t list_current_len = 0u;
   {
       char const *p = ll->tail->buf;
       while ( TERM != *p ) ++p;
       list_current_len = p - ll->tail->buf;
   }

   //printf("list_current_len = %zu\n", list_current_len);

   size_t const list_free_space = BUFSIZNODE - list_current_len - 1u;
   if ( arglen < list_free_space )
   {
       //printf("--- Concatenating to current tail\n");
       strcpy(ll->tail->buf + list_current_len, arg);
       return;
   }

   printf("--- Creating a new tail\n");
   struct Node *const newtail = NewNode(arg);
   if ( NULL == newtail ) abortwhy("Failed to allocate new tail");
   ll->tail->next = newtail;
   ll->tail = newtail;
}

static size_t List_find(struct List const *const ll, char const *const arg)
{
   if ( NULL==ll || NULL==arg ) return 0;

   size_t index = 0u;
   for ( struct Node const *n = ll->head; NULL != n; n = n->next )
   {
       for ( char const *p = n->buf; TERM != *p; Increment_Past_Null(&p) )
       {
           if ( 0 == strcmp(p,arg) ) return index;
           ++index;
       }
   }

   return -1;
}

int Polymorphism_Process_Symbol(char const *const arg, size_t const offset)
{
    if ( NULL == arg ) return 0;

    if ( strlen(arg) < 6u ) return 0;

    /**/ if ( 0 == strncmp("_ZTV", arg, 4u) ) List_append( &g_vtables  , arg + 4u, offset );
    else if ( 0 == strncmp("_ZTI", arg, 4u) ) List_append( &g_typeinfos, arg + 4u, offset );
    else return 0;

    return 1;
}

static size_t hash_code_from_typeinfo_name(char const *const buf);  /* defined lower down in this file */

void Polymorphism_Print_Summary(void)
{
   if ( NULL == g_vtables.head )
   {
      printf("list of vtables is empty\n");
      return;
   }

   size_t index_offsets_vtables = -1;

   for ( struct Node const *n = g_vtables.head; NULL != n; n = n->next )
   {
      char const *p = n->buf;
      for ( ; TERM != *p; Increment_Past_Null(&p) )
      {
          ++index_offsets_vtables;
          size_t const index_offsets_typeinfos = List_find(&g_typeinfos,p);
          if ( (size_t)-1 == index_offsets_typeinfos ) continue;
          printf("_Z%s", p);
#if 0  // This code is crashing -- don't know why
          char *const tmp = malloc( strlen(p) + 2u );
          if ( NULL == tmp )
          {
              printf("OUT OF MEMORY\n");
              return;
          }
          tmp[0u] = '_'; tmp[1u] = 'Z'; tmp[2u] = '\0';
          strcpy(tmp + 2u, p);
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
          free(tmp);
          //printf(" - tmp freed - ");
#endif
          printf(", hash_code = %zu,", hash_code_from_typeinfo_name(p));
          ptrdiff_t const pdV = GetOffset(&g_vtables  ,index_offsets_vtables  );
          ptrdiff_t const pdT = GetOffset(&g_typeinfos,index_offsets_typeinfos);
          ptrdiff_t const delta = pdV - pdT;
          printf(", address of vtable = address of typeinfo + %td bytes\n",delta);
      }
   }
}

static size_t hash_code_from_typeinfo_name(char const *const buf)
{
    static size_t const mul = (((size_t)0xc6a4a793U) << 32U) + (size_t)0x5bd1e995U;
    // Remove the bytes not divisible by the sizeof(size_t).  This
    // allows the main loop to process the data as 64-bit integers.
    size_t const len = strlen(buf);
    size_t const len_aligned = len & ~(size_t)0x7;
    char const *const end = buf + len_aligned;
    size_t hash = 0xc70f6907U ^ (len * mul);
    for ( char const *p = buf; end != p; p += 8U )
    {
        size_t to_be_shiftmixed;
        __builtin_memcpy(&to_be_shiftmixed,p,sizeof(size_t));
        to_be_shiftmixed *= mul;
        size_t const data = (to_be_shiftmixed ^ (to_be_shiftmixed >> 47U)) * mul;
        hash ^= data;
        hash *= mul;
    }
    if ( 0U != (len & 0x7) )
    {
        int n = len & 0x7;
        size_t data = 0U;
        --n;
        do { data = (data << 8u) + (char unsigned)end[n]; } while ( --n >= 0 );
        hash ^= data;
        hash *= mul;
    }
    hash = (hash ^ (hash >> 47U)) * mul;
    hash = (hash ^ (hash >> 47U));
    return hash;
}
