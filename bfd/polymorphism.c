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
extern char *xstrdup(const char*);

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

struct MappingTypeinfoVtable {
    uint32_t offset_typeinfo, offset_vtable;
};

static struct MappingTypeinfoVtable *g_polymap = NULL;
static uint32_t g_polymap_size = 0u;

struct ListSymbolNode {
    struct ListSymbolNode *next;
    char *name;
    uint32_t offset;
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

static uint32_t Count_Pairs(void)
{
    uint32_t count = 0u;

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

int Polymorphism_Process_Symbol_2nd_Run(char const *const arg, uint32_t const offset)
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
    uint32_t index = -1;

    //printf("std::pair< std::size_t, std::size_t > map_typeinfo_vtable[] = {\n");
    for ( struct ListSymbolNode const *nv = g_vtables.head; NULL != nv; nv = nv->next )
    {
        struct ListSymbolNode const *const ni = ListSymbol_find(&g_typeinfos,nv->name);
        if ( NULL == ni ) continue;
        ++index;
        g_polymap[index].offset_typeinfo = ni->offset;
        g_polymap[index].offset_vtable   = nv->offset;
        //printf("  { %*zu, %*td },  // _Z%s\n", 20, g_polymap[index].offset_typeinfo, 20, g_polymap[index].offset_vtable, nv->name);
    }
    //puts("};");
}

static int compare(void const *const va, void const *const vb)
{  
    struct MappingTypeinfoVtable const *const a = va,
                                       *const b = vb;

    if ( (0u==a->offset_typeinfo && 0u==a->offset_vtable) && (0u==b->offset_typeinfo && 0u==b->offset_vtable) ) return 0;

    if ( 0u==a->offset_typeinfo && 0u==a->offset_vtable ) return +1;

    if ( 0u==b->offset_typeinfo && 0u==b->offset_vtable ) return -1;

    if ( a->offset_typeinfo == b->offset_typeinfo ) abortwhy("Two typeinfos cannot have the same address");    // but vtables can

    return (a->offset_typeinfo < b->offset_typeinfo) ? -1 : +1;
}

uint32_t Polymorphism_Finalise_Symbols_And_Create_Map(void **const pp)
{
    *pp = NULL;
    g_polymap_size = Count_Pairs();
    if ( 0u == g_polymap_size ) return 0u;
    g_polymap = xmalloc(g_polymap_size * sizeof(struct MappingTypeinfoVtable));
    Polymorphism_Populate_Map_Numbers();

    qsort(g_polymap, g_polymap_size, sizeof *g_polymap, compare);

    for ( unsigned i = 0u; i < g_polymap_size; ++i )
    {
        if ( 0u==g_polymap[i].offset_typeinfo && 0u==g_polymap[i].offset_vtable )
        {
            g_polymap_size = i;
            break;
        }
    }

    /*
    printf("std::pair< std::size_t, std::size_t > map_typeinfo_vtable[] = {\n");
    for ( size_t i = 0u; i < g_polymap_size; ++i )
    {
        printf("  { %*zu, %*td },\n", 20, g_polymap[i].offset_typeinfo, 20, g_polymap[i].offset_vtable);
    }
    puts("};");
    */

    // ========================================== Begin freeing memory
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
    // ========================================== End freeing memory

    *pp = g_polymap;
    return g_polymap_size * sizeof(struct MappingTypeinfoVtable);
}

char unsigned const (*Polymorphism_Get_128_Random(void))[16u]
{
    static char unsigned buf[16u];
    static int already_done = 0;
    if ( already_done ) return &buf;
    already_done = 1;
    srand(time(0));
    for( unsigned i = 0u; i < 16u; ++i ) buf[i] = rand() % 256u;
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
    uint32_t const count = Count_Pairs();
    uint32_t const count_bytes = count * sizeof(struct MappingTypeinfoVtable);

    Polymorphism_Get_Name_Extra_Object_File();  // in case it wasn't already called
    char *const pdot = g_extra_object_file + strlen(g_extra_object_file) - 2u;

    *pdot = '\0';
    FILE *const f = fopen(g_extra_object_file, "w");
    *pdot = '.';
    if ( NULL == f ) abort();

    fprintf(f,
            "extern char const __map_typeinfo_vtable[4u + 4u + %luu];\n"
            "       char const __map_typeinfo_vtable[4u + 4u + %luu] = {\n  ",
            (long unsigned)count_bytes,
            (long unsigned)count_bytes);

    char unsigned const (*const mybytes)[16u] = Polymorphism_Get_128_Random();
    for ( unsigned i = 0u; i < 16u; ++i )
    {
        fprintf(f, "0x%02x, ", (unsigned)(*mybytes)[i]);
    }
    fprintf(f, "  /* These first 16 bytes are the random 128-Bit number */\n  ");

    char unsigned val = 0u;
    for ( size_t i = 0u; i < (count_bytes + 4u + 4u - 16u); ++i )
    {
        fprintf(f, "0x%02x, ", (unsigned)val++);
        if ( 0u == (val % 16u) ) fprintf(f, "\n  ");
    }
    fprintf(f,"\n};\n");
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
