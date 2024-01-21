#include "polymorphism.h"
#include <assert.h>                             // assert
#include <stdlib.h>                             // abort
#include <stddef.h>                             // NULL
#include <string.h>                             // memset, strlen
#include <stdio.h>                              // fprintf
#include <threads.h>                            // once_flag, call_once

extern char *cplus_demangle_v3 (const char *mangled, int options);

struct node {
   struct node *next;
   char buf[1u];
};

struct LinkedList {
    struct node *head, *tail;
};

static struct LinkedList g_vtables = {0,0}, g_typeinfos = {0,0};

static void LinkedList_insert(struct LinkedList *const ll, char const *const arg)
{
   struct node *const link = malloc( sizeof(struct node) + strlen(arg) );
   link->next = NULL;
   strcpy(link->buf, arg);

   if ( NULL == ll->head )
   {
       ll->head = ll->tail = link;
       return;
   }

   ll->tail->next = link;
   ll->tail = link;
}

static int LinkedList_find(struct LinkedList const *const ll, char const *const arg)
{
   if ( NULL == ll ) return 0;

   struct node const *p = ll->head;

   while ( NULL != p )
   {        
      if ( 0 == strcmp(arg, p->buf) ) return 1;
      p = p->next;
   }

    return 0;
}

int Polymorphism_Process_Symbol(char const *const arg)
{
    if ( NULL == arg ) return 0;

    if ( strlen(arg) < 6u ) return 0;

    /**/ if ( 0 == strncmp("_ZTV", arg, 4u) ) LinkedList_insert( &g_vtables  , arg + 4u);
    else if ( 0 == strncmp("_ZTI", arg, 4u) ) LinkedList_insert( &g_typeinfos, arg + 4u);
    else return 0;

    return 1;
}

size_t hash_code_from_typeinfo_name(char const *const buf);  /* defined lower down in this file */

void Polymorphism_Print_Summary(void)
{
   struct node const *p = g_vtables.head;

   if ( NULL == p )
   {
      printf("list is empty\n");
      return;
   }

   for ( ; NULL != p; p = p->next )
   {        
      if ( 0 == LinkedList_find(&g_typeinfos, p->buf) ) continue;
      printf("_Z%s - ", p->buf);
      char *const tmp = malloc( strlen(p->buf) + 2u );
      if ( NULL == tmp )
      {
          printf("OUT OF MEMORY\n");
          return;
      }
      tmp[0u] = '_'; tmp[1u] = 'Z'; tmp[2u] = '\0';
      strcpy(tmp + 2u, p->buf);
#if 0
      char *const demangled = cplus_demangle_v3(tmp,0); //DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE);
      if ( NULL != demangled )
      {
          printf("%s", demangled);
          if ( tmp != demangled )
          {
              //printf(" - about to free demangled - ");
              free(demangled);
              //printf(" - demangled freed - ");
          }
      }
#endif
      //printf(" - about to free tmp - ");
      free(tmp);
      //printf(" - tmp freed - ");
      printf(", hash_code = %zu\n", hash_code_from_typeinfo_name(p->buf));
   }
}

size_t hash_code_from_typeinfo_name(char const *const buf)
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

