#include "polymorphism.h"
#include <assert.h>                             // assert
#include <stddef.h>                             // NULL, size_t
#include <stdint.h>                             // uint32_t
#include <stdlib.h>                             // abort, rand, srand, qsort
#include <string.h>                             // memcmp, strcmp, strlen
#include <stdio.h>                              // printf
#include <time.h>                               // time(0)

extern char *cplus_demangle_v3 (const char *mangled, int options);   // defined in libiberty/cp-demangle.c
extern void *xmalloc(size_t);
extern char *xstrdup(char const*);

static char g_extra_object_file[256u] = "/tmp/linker_map_typeinfo_vtable_";

static void abortwhy(char const *const p)
{
    printf("\n========== abort() called - '%s' ==========\n", p);
    abort();
}

char **Duplicate_Argv_Plus_Extra(int const argc, char **argv, char const *const extra)
{
    char **const new_argv = xmalloc( (argc+2u) * sizeof *new_argv );
    for ( int i = 0u; i < argc; ++i ) new_argv[i] = argv[i];
    new_argv[argc + 0u] = (char*)extra;
    new_argv[argc + 1u] = NULL;
    return new_argv;
}

static char *g_polymap = NULL;
static size_t g_polymap_size = 0u;

struct ListSymbolNode {
    struct ListSymbolNode *next;
    char const *name;
};

struct ListSymbol {
    struct ListSymbolNode *head, *tail;
};

struct ListSymbol g_vtables = {NULL,NULL}, g_typeinfos = {NULL,NULL};

static struct ListSymbolNode *NewListSymbolNode(char const *const arg)
{
    struct ListSymbolNode *const p = xmalloc( sizeof(struct ListSymbolNode) );
    p->next = NULL;
    p->name = xstrdup(arg);
    char *const atsymbol = strchr(p->name, '@');
    if ( NULL != atsymbol ) *atsymbol = '\0';
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

bool Polymorphism_Process_Symbol_1st_Run(char const *const arg)
{
    if ( NULL == arg ) return false;

    if ( strlen(arg) < 6u ) return false;

    /**/ if ( 0 == strncmp("_ZTV", arg, 4u) ) ListSymbol_append( &g_vtables  , arg + 4u );
    else if ( 0 == strncmp("_ZTI", arg, 4u) ) ListSymbol_append( &g_typeinfos, arg + 4u );
    else return 0;

    return true;
}

static void Polymorphism_Finalise_Symbols_And_Create_Map(void)
{
    assert(   0u == g_polymap_size );
    assert( NULL == g_polymap      );

    size_t len = 0u;

    for ( struct ListSymbolNode const *nv = g_vtables.head; NULL != nv; nv = nv->next )
    {
        struct ListSymbolNode const *const ni = ListSymbol_find(&g_typeinfos,nv->name);
        if ( NULL == ni ) continue;
        len += strlen(nv->name) + 1u;  // +1 for the null terminator
        ++g_polymap_size;
    }

    if ( 0u == g_polymap_size ) return;
    g_polymap = xmalloc(len);
    g_polymap[0u] = '\0';

    char *start = g_polymap;
    for ( struct ListSymbolNode const *nv = g_vtables.head; NULL != nv; nv = nv->next )
    {
        struct ListSymbolNode const *const ni = ListSymbol_find(&g_typeinfos,nv->name);
        if ( NULL == ni ) continue;
        strcpy( start, nv->name );
        start += strlen(start) + 1u;
    }

    assert( '\0' != g_polymap[0u] );

    // ========================================== Begin freeing memory
    for ( struct ListSymbol *ls = &g_vtables; ; ls = &g_typeinfos )
    {
        for ( struct ListSymbolNode *n = ls->head; NULL != n; )
        {
            free( (char*)n->name);
            void *const to_be_freed = n;
            n = n->next;
            free( to_be_freed );
        }
        if ( &g_typeinfos == ls ) break;
    }
    g_vtables.head = g_vtables.tail = g_typeinfos.head = g_typeinfos.tail = NULL;
    // ========================================== End freeing memory
}

static char unsigned const (*Polymorphism_Get_128_Random(void))[16u]
{
    static char unsigned buf[16u];
    static bool already_done = false;
    if ( already_done ) return &buf;
    already_done = true;
    extern ssize_t getrandom(void *buffer, size_t length, unsigned int flags);
    getrandom(buf, 16u, /*GRND_RANDOM*/ 2u);
    return &buf;
}

char const *Polymorphism_Get_Name_Extra_Object_File(void)  // string returned must end with ".c.o"
{
#if 0
    strcpy(g_extra_object_file,"/tmp/monkey.c.o");
#else
    size_t const len = strlen(g_extra_object_file);
    if ( 'o' == g_extra_object_file[len - 1u] ) return g_extra_object_file;
    char *const p = g_extra_object_file + len;
    static char const digits[] = "0123456789abcdef";
    char unsigned const (*const mybytes)[16u] = Polymorphism_Get_128_Random();
    for( unsigned i = 0u; i < 16u; ++i )
    {
        p[2u*i + 0u] = digits[ 0xF & ((*mybytes)[i] >> 4u) ];
        p[2u*i + 1u] = digits[ 0xF & ((*mybytes)[i] >> 0u) ];
    }
    p[32u] = '\0';
    strcat( g_extra_object_file, ".c.o" );
#endif
    FILE *const f = fopen(g_extra_object_file, "w");
    if ( NULL == f ) return NULL;
    fclose(f);
    return g_extra_object_file;
}

char const *Polymorphism_Create_Extra_Object_File(void)
{
    Polymorphism_Finalise_Symbols_And_Create_Map();  // in case it wasn't already called
    char *const pdot = g_extra_object_file + strlen(g_extra_object_file) - 2u;

    if ( '.' != *pdot ) abortwhy("The string 'g_extra_object_file' is corrupt");
    *pdot = '\0';
    FILE *const f = fopen(g_extra_object_file, "w");
    *pdot = '.';
    if ( NULL == f ) abortwhy("fopen failed to open the extra object file");

    fprintf(f,
            "extern long long unsigned const __map_typeinfo_vtable_size __attribute__((section(\".data.rel.ro\")));\n"
            "       long long unsigned const __map_typeinfo_vtable_size = %lluu;\n\n",
            (long long unsigned)g_polymap_size);

    char *p = g_polymap;
    for ( size_t i = 0u; i < g_polymap_size; ++i )
    {
        fprintf(f,"extern void const *const *const _ZTI%s;\n"
                  "extern void const *const *const _ZTV%s;\n",p,p);
        p += strlen(p) + 1u;
    }

    fprintf(f,
            "\n"
            "extern void const *const __map_typeinfo_vtable[%lluu][2u] __attribute__((section(\".data.rel.ro\")));\n"
            "       void const *const __map_typeinfo_vtable[%lluu][2u] = {\n",
            (long long unsigned)g_polymap_size,
            (long long unsigned)g_polymap_size);

    p = g_polymap;
    for ( size_t i = 0u; i < g_polymap_size; ++i )
    {
        fprintf(f,"  { &_ZTI%s , &_ZTV%s },\n",p,p);
        p += strlen(p) + 1u;
    }

    fprintf(f,"};\n");
    fclose(f);

    char str[512u] = "gcc -s -O3 -DNDEBUG -c ";
    *pdot = '\0';
    strcat(str, g_extra_object_file);
    *pdot = '.';
    strcat(str, " -o ");
    strcat(str, g_extra_object_file);
    int const dummy = system(str);
    (void)dummy;
    return g_extra_object_file;
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
