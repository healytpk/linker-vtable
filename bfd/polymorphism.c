#include "polymorphism.h"
#include <assert.h>                             // assert
#include <stdlib.h>                             // abort
#include <stddef.h>                             // NULL
#include <string.h>                             // memset, strlen
#include <stdio.h>                              // fprintf
#include <threads.h>                            // once_flag, call_once

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

void Polymorphism_Print_Summary(void)
{
   struct node const *p = g_vtables.head;

   if ( NULL == p )
   {
      printf("list is empty\n");
      return;
   }

   while ( NULL != p )
   {        
      if ( LinkedList_find(&g_typeinfos, p->buf) )
      {
#if 0
          printf("%s\n", p->buf);
#else
          static char cmd[1024u];
          strcpy(cmd, "c++filt _Z");
          strcat(cmd, p->buf);
          char *const at = strchr(cmd, '@');
          if ( NULL != at ) *at = '\0';
          if ( system(cmd) ) {}
#endif
      }

      p = p->next;
   }
}
