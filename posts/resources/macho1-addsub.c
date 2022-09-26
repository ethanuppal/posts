// main.c: A program to generate a simple mach-o file which contains the code
//         for a linkable add function.
// Copyright 2021 (C) Ethan Uppal. All rights reserved.

#include <stdio.h>
#include <sys/mman.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define S(name) struct name
#define guard(condition) if (condition) {}
#define try(name, ...) if (name(__VA_ARGS__) != 0) return (perror(#name), 1)

int main2(int argc, const char* argv[]) {
    // Size of all load commands
    const size_t size_of_cmds = (sizeof(S(segment_command_64))
                                    + sizeof(S(section_64)) // we have one sect
                                 + sizeof(S(build_version_command))
                                 + sizeof(S(symtab_command))
                                 + sizeof(S(dysymtab_command)));

    // End location of the load commands
    const size_t end_of_cmds = size_of_cmds + sizeof(S(mach_header_64));

    // The x86_64 for two functions that add and subtract two 32-bit integers
    const unsigned char bytecode_add[] = {
                    // _add:
        0x01, 0xF7, //   add edi, esi ; first two args in SysV
        0x89, 0xF8, //   mov eax, edi ; return value in eax
        0xC3,       //   ret          ; exit function
    };
    const unsigned char bytecode_sub[] = {
                    // _sub:
        0x29, 0xF7, //   sub edi, esi ; first two args in SysV
        0x89, 0xF8, //   mov eax, edi ; return value in eax
        0xC3,       //   ret          ; exit function
    };
    #define SZ_BYTECODE (sizeof(bytecode_add) + sizeof(bytecode_sub))

    // The string table for symbol reference
    const char string_table[] = {
        '\0', // first string empty
        '_', 'a', 'd', 'd', '\0', // _add
        '_', 's', 'u', 'b', '\0', // _sub
        '\0' // end of argz
    };

    // File header
    S(mach_header_64) header = {
        .magic = MH_MAGIC_64,
        .cputype = CPU_TYPE_X86_64,
        .cpusubtype = CPU_SUBTYPE_X86_64_ALL,
        .filetype = MH_OBJECT,
        .ncmds = 4,
        .sizeofcmds = size_of_cmds,
        .flags = 0
    };

    // Load command #1: __TEXT segment
    S(segment_command_64) segment = {
        .cmd = LC_SEGMENT_64,
        .cmdsize = sizeof(S(segment_command_64)) + sizeof(S(section_64)),
        .vmaddr = 0,
        .vmsize = SZ_BYTECODE, // only one section
        .fileoff = end_of_cmds,
        .filesize = SZ_BYTECODE,
        .initprot = PROT_READ | PROT_WRITE | PROT_EXEC,
        .maxprot = PROT_READ | PROT_WRITE | PROT_EXEC,
        .nsects = 1,
        .flags = 0
    };

    // The code (__text) section inside the segment
    S(section_64) section = {
        .sectname = "__text",
        .segname = "__TEXT",
        .addr = 0,
        .size = SZ_BYTECODE,
        .offset = end_of_cmds, // we want the bytecode right after the cmds
        .align = 0,
        .reloff = 0,
        .nreloc = 0,
        .flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS
    };

    // Load command #2: Tells the linker what version this is for
    S(build_version_command) build_version = {
        .cmd = LC_BUILD_VERSION,
        .cmdsize = sizeof(build_version),
        .platform = PLATFORM_MACOS,
        .minos = 0x000a0f00, // 10.15.0
        .sdk = 0, // 0.0.0
        .ntools = 0 //  no build tools
    };

    // The symbol we refer to for _add
    S(nlist_64) symbol_add = {
        .n_un = { .n_strx = 1 }, // offset 1 in string table
        .n_type = N_SECT | N_EXT, // defined in section & external symbol
        .n_sect = 1, // index from 1
        .n_desc = 0,
        .n_value = 0
    };

    // The symbol we refer to for _sub
    S(nlist_64) symbol_sub = {
        .n_un = { .n_strx = 6 }, // offset 6 in string table
        .n_type = N_SECT | N_EXT, // defined in section & external symbol
        .n_sect = 1, // index from 1
        .n_desc = 0,
        .n_value = sizeof(bytecode_add)
    };

    S(nlist_64) symbols[] = {
        symbol_add, symbol_sub
    };

    // Load command #3: The symbol table command
    S(symtab_command) symtab = {
        .cmd = LC_SYMTAB,
        .cmdsize = sizeof(symtab),
        .symoff = end_of_cmds + SZ_BYTECODE,
        .nsyms = sizeof(symbols)/sizeof(S(nlist_64)),
        .stroff = end_of_cmds + SZ_BYTECODE + sizeof(symbols),
        .strsize = sizeof(string_table)
    };

    // Load command #4: The dynamic symbol table command
    S(dysymtab_command) dysymtab = {
        .cmd = LC_DYSYMTAB,
        .cmdsize = sizeof(dysymtab),
        .nextdefsym = 1,
        .iextdefsym = 1
        // everything else should be 0
    };

    // Create a file to write to
    FILE* file = fopen("test.o", "w");
    guard (file) else {
        perror("fopen");
        return 1;
    }

    // Write all the contents
    guard (fwrite(&header, sizeof(header), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&segment, sizeof(segment), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&section, sizeof(section), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&build_version, sizeof(build_version), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&symtab, sizeof(symtab), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&dysymtab, sizeof(dysymtab), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(bytecode_add, sizeof(bytecode_add), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(bytecode_sub, sizeof(bytecode_sub), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(symbols, sizeof(symbols), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(string_table, sizeof(string_table), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }

    // Close the file
    try(fclose, file);

    return 0;
}
