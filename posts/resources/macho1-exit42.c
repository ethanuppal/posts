// main.c: A program to generate a simple mach-o file which contains the code
//         for exit(42).
// Copyright (C) 2021 Ethan Uppal. All rights reserved.

#include <stdio.h>
#include <sys/mman.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define S(name) struct name
#define guard(condition) if (condition) {}
#define try(name, ...) if (name(__VA_ARGS__) != 0) return (perror(#name), 1)

int main(int argc, const char* argv[]) {
    // Size of all load commands
    const size_t size_of_cmds = (sizeof(S(segment_command_64))
                                    + sizeof(S(section_64)) // we have one sect
                                 + sizeof(S(build_version_command))
                                 + sizeof(S(symtab_command))
                                 + sizeof(S(dysymtab_command)));

    // End location of the load commands
    const size_t end_of_cmds = size_of_cmds + sizeof(S(mach_header_64));

    // The x86_64 to perform exit(42)-with-extra-steps on mac
    const unsigned char bytecode[] = {
                                      // _main:
        0xeb, 0x05,                   //   jmp _main_foo
        0x90, 0x90, 0x90, 0x90, 0x90, //   nop \n nop \n nop \n nop \n nop
                                      // _main_foo:
        0xb8, 0x01, 0x00, 0x00, 0x02, //   mov eax, 0x2000001 ; exit syscall
        0xbf, 0x2a, 0x00, 0x00, 0x00, //   mov edx, 42        ; exit code
        0x0f, 0x05,                   //   syscall            ; perform syscall
    };

    // The string table for symbol reference
    const char string_table[] = {
        '\0', // first string empty
        '_', 'm', 'a', 'i', 'n', '\0', // _main
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
        .vmsize = sizeof(bytecode), // only one section; section.size
        .fileoff = 0x138,
        .filesize = sizeof(bytecode),
        .initprot = 0,
        .maxprot = 0,
        .nsects = 1,
        .flags = 0
    };

    // The code (__text) section inside the segment
    S(section_64) section = {
        .sectname = "__text",
        .segname = "__TEXT",
        .addr = 0,
        .size = sizeof(bytecode),
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
        .platform = 1, // not sure what this means
        .minos = 0, // 0x000a0f00 10.15.0
        .sdk = 0, // 0.0.0
        .ntools = 0 //  no build tools
    };

    // Load command #3: The symbol table command
    S(symtab_command) symtab = {
        .cmd = LC_SYMTAB,
        .cmdsize = sizeof(symtab),
        .symoff = end_of_cmds + sizeof(bytecode),
        .nsyms = 1,
        .stroff = end_of_cmds + sizeof(bytecode) + sizeof(S(nlist_64)),
        .strsize = sizeof(string_table)
    };

    // The symbol we refer to for _main
    S(nlist_64) symbol = {
        .n_un = { .n_strx = 1 }, // second string in table
        .n_type = 0xF, // not sure what this means
        .n_sect = 1, // index from 1
        .n_desc = 0
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
    guard (fwrite(&bytecode, sizeof(bytecode), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&symbol, sizeof(symbol), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }
    guard (fwrite(&string_table, sizeof(string_table), 1, file) == 1) else {
        perror("fwrite");
        return 1;
    }

    // Close the file
    try(fclose, file);

    return 0;
}
