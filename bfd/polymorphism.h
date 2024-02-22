#include <stdbool.h>   // bool, true, false
#include <stddef.h>    // size_t
#include <stdio.h>     // FILE
#include <stdint.h>    // uint32_t

extern uint32_t g_offset_of_map_typeinfo_vtable;

extern char **Duplicate_Argv_Plus_Extra(int const argc, char **argv, char const *const extra);

extern char unsigned const (*Polymorphism_Get_128_Random(void))[16u];  // Allow for (16==CHAR_BIT)

extern bool Polymorphism_Process_Symbol_1st_Run(char const*);

extern char const *Polymorphism_Get_Name_Extra_Object_File(void);
extern char const *Polymorphism_Create_Extra_Object_File(void);

extern size_t Polymorphism_Finalise_Symbols_And_Create_Map(void**);
extern bool Polymorphism_Seek_To_UUID_In_File(FILE *const f);
