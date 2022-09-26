# I Hate Mach-O

> This is part one of a two-part series. [Part two](/posts/macho2.md) describes Mach-O files in detail. These posts aim to help compiler designers who wish to emit Mach-O.

Take the following code:
```c
int add(int x, int y) {
    return x + y;
}
```
You cannot compile and link it for execution because there is no entry point, i.e., no `main` function. However, you do not need to run this file. Unless you want a massive, hard-to-debug file, you need to represent the partial compilation of each smaller file. Enter object files: they contain symbols and metadata alongside the machine code of the fragment. A linker uses this extra information to stitch them into a complete output.

Every established platform has its object file format. Windows has [COFF (Common Object File Format)](https://docs.microsoft.com/en-us/windows/win32/debug/pe-format), Linux has [ELF (Executable & Linkable Format)](https://refspecs.linuxfoundation.org/elf/elf.pdf), and macOS has Mach-O. You might notice that I did not provide a reference link for Mach-O files. Unfortunately, there is none.

## Pain, Pain and More Pain

The only official "documentation" on the Apple website that I could locate is [here](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/CodeFootprint/Articles/MachOOverview.html#//apple_ref/doc/uid/20001860-100029-TPXREF104). When you visit this page, you are met with the following warning:

> **Retired Document**  
> Important: This document may not represent best practices for current development. Links to downloads and other resources may no longer be valid.

Thanks, Apple.

In fact, the most recent official reference [is from 2009](https://github.com/aidansteele/osx-abi-macho-file-format-reference/blob/master/Mach-O_File_Format.pdf) and has since been deleted. This link is to someone's GitHub archive of the documentation; I cannot be assured of its reliability.

Therefore, when I tried to write a compiler for macOS, I had no place to start. Given how many resources there are for COFF and ELF files, I assumed it would be just as simple for MacOS. I could not have been more wrong. To illustrate what I was facing, [a 2006 publication](https://www.cs.miami.edu/home/burt/learning/Csc521.091/docs/MachOTopics.pdf) by Apple explains that "[t]he Xcode Tools CD contains several command-line tools (which this document refers to collectively as the standard tools) for building and analyzing your application" and elaborates no further. Apple seems to want Apple developers to use only Apple software to design Apple products–surprising! 🍎

I was not discouraged yet. A few more searches yielded [this article](https://lowlevelbits.org/parsing-mach-o-files/), a gold mine that helped me develop an initial understanding of Mach-O files. However, the post only explained how to print segment names, and the example program did not work for me.

I continued searching for information, but I encountered the same links day after day. I even considered generating ELF instead because, although I could not test it, I would at least have an iota of hope creating them. Eventually, I grew tired of the lack of progress and quit.

## Reverse Engineering

After my hiatus, I once again became determined to finish my compiler–I would solve the Mach-O riddle or die trying. I delved into the confusing and unclear 2009 Apple docs (or potentially the invention of a GitHub user!) and tried to piece together what they meant. Using the docs and my preliminary knowledge, I generated some quasi-Mach-O files–even succeeding in them being half-recognized by `objdump`–but `gcc` complained about various inconsistencies. As usual, there was nowhere I could consult.

This first plunge into the deep end had borne some fruit: I had figured out the general structure of a Mach-O file. There is a header, a series of load commands, and data. I had even associated some of these elements with such C structures as `struct mach_header_64` and `struct load_command_64`.

_But how was I to use them?_ I had no idea what contents these elements should have. The Apple docs provided options for some fields, but no method for deciding what options to use. Here are some examples:

### Hall of Shame

> [`stroff`](https://github.com/aidansteele/osx-abi-macho-file-format-reference#fields-22)  
> An integer containing the byte offset from the start of the image to the location of the string table.

The term "image" is never defined, and the phrase "byte offset from the start of the file" is used separately.

> [`cpusubtype`](https://github.com/aidansteele/osx-abi-macho-file-format-reference#fields-1)  
> An integer specifying the exact model of the CPU. To run on all PowerPC processors supported by the OS X kernel, set it to CPU_SUBTYPE_POWERPC_ALL.

Great! I could not care less about PowerPC. What about x86? Ah, I forgot–documentation is not designed to be helpful!

The [documentation for `nlist_64`](https://developer.apple.com/documentation/kernel/nlist_64/1583957-n_desc) has the following explanation for the member `n_desc`:

> A 16-bit value providing additional information about the nature of this symbol. The reference flags can be accessed using the REFERENCE_TYPE mask (0xF) and are defined as follows:

The explanation ends without defining the reference flags, so I had to dig into the header file.

### Light at the End of the Tunnel

In the face of these inconveniences, an idea came to me for how I could solve them.

My idea was simple: I would write a program that prints out the entirety of any given 64-bit Mach-O file (I only plan to target 64-bit architectures in my compiler, and I doubt that anyone is running macOS on an embedded system). You can find the result [on GitHub](https://github.com/ethanuppal/machdump).

![Example output of my Mach-O debug tool](https://raw.githubusercontent.com/ethanuppal/machdump/main/img/example.png)
_Example output of my Mach-O debug tool_

I entered the battlefield with `printf` at the ready. First to be conquered was the header, then the load commands. After these exploits, I went deeper into the load commands to extract specific information about them. For instance, `struct section_64` contains a member related to the machine code. I tried printing the bytes at the offset that the member specified, and they matched the assembly code I had inputted. I did the same print-and-test game with `struct nlist_64` to discover where symbols were stored. As I put more and more of these pieces together, I slowly began to understand the internals of Mach-O files. I created various assembly scripts isolating a single aspect of compilation (the code itself, symbols, multiple symbols, etc.) and ran my program on them to learn what the Mach-O fields became.

Finally, I was able to piece together all the components and wrote [a 167-line C script that generates a Mach-O file for `exit(42)`](/posts/resources/macho1-exit42.c). After more head-scratching and a reread of the docs, I produced [another C script that generates a Mach-O file containing `add` and `sub` functions](/posts/resources/macho-addsub.c). This file, in particular, can link with a C file such as the following:

```c
int add(int x, int y);
int sub(int x, int y);

int main() {
    // Use these functions
}
```

Although I still have much more to learn about Mach-O files, my journey to understand them ends here. With the foundational knowledge I have acquired, I can more easily look deeper into the complexities that I ignored during this process. In [a part two](/posts/macho2.md), I will provide a more in-depth analysis of Mach-O files and walk through that first `exit(42)` program.

_Copyright (C) 2021 Ethan Uppal. This work is licensed under a [Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/.)._
