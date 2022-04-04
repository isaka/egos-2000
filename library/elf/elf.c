/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: load an ELF file into memory
 * Only using the program header instead of the multiple section headers
 */

#include "egos.h"
#include "elf.h"
#include "disk.h"
#include "memory.h"
#include "servers.h"

#include <string.h>

static void load_grass(elf_reader reader,
                       struct elf32_program_header* pheader) {
    INFO("Grass kernel file size: 0x%.8x bytes", pheader->p_filesz);
    INFO("Grass kernel memory size: 0x%.8x bytes", pheader->p_memsz);

    if (pheader->p_filesz > GRASS_SIZE ||
        pheader->p_offset % BLOCK_SIZE != 0)
        FATAL("Invalid grass binary file");

    char* entry = (char*)GRASS_ENTRY;
    int block_offset = pheader->p_offset / BLOCK_SIZE;
    for (int off = 0; off < pheader->p_filesz; off += BLOCK_SIZE)
        reader(block_offset++, entry + off);

    memset(entry + pheader->p_filesz, 0, GRASS_SIZE - pheader->p_filesz);
}

static void load_app(int pid, elf_reader reader,
                     int argc, void** argv,
                     struct elf32_program_header* pheader) {

    if (pid < GPID_USER_START) {
        INFO("App file size: 0x%.8x bytes", pheader->p_filesz);
        INFO("App memory size: 0x%.8x bytes", pheader->p_memsz);
    }

    if (pheader->p_filesz > APPS_SIZE ||
        pheader->p_offset % BLOCK_SIZE != 0)
        FATAL("Invalid app binary file");

    /* load the app code & data */
    int base, frame_no, page_no = 0;
    int block_offset = pheader->p_offset / BLOCK_SIZE;
    for (int off = 0; off < pheader->p_filesz; off += BLOCK_SIZE) {
        if (off % PAGE_SIZE == 0) {
            earth->mmu_alloc(&frame_no, &base);
            earth->mmu_map(pid, page_no++, frame_no);
        }
        reader(block_offset++, ((char*)base) + (off % PAGE_SIZE));
    }
    int last_page_filled = pheader->p_filesz % PAGE_SIZE;
    int last_page_nzeros = PAGE_SIZE - last_page_filled;
    if (last_page_filled)
        memset(((char*)base) + last_page_filled, 0, last_page_nzeros);

    while (page_no < APPS_SIZE / PAGE_SIZE) {
        earth->mmu_alloc(&frame_no, &base);
        earth->mmu_map(pid, page_no++, frame_no);
        memset((char*)base, 0, PAGE_SIZE);
    }

    /* allocate two pages for argc, argv and stack */
    earth->mmu_alloc(&frame_no, &base);
    earth->mmu_map(pid, page_no++, frame_no);

    /* base is at virtual address APPS_ARG */
    int* argc_addr = (int*)base;
    int* argv_addr = argc_addr + 1;
    int* args_addr = argv_addr + CMD_NARGS;

    *argc_addr = argc;
    if (argv) memcpy(args_addr, argv, argc * CMD_ARG_LEN);
    for (int i = 0; i < argc; i++)
        argv_addr[i] = (int)((char*)args_addr + i * CMD_ARG_LEN);

    earth->mmu_alloc(&frame_no, &base);
    earth->mmu_map(pid, page_no++, frame_no);
}

void elf_load(int pid, elf_reader reader, int argc, void** argv) {
    char buf[BLOCK_SIZE];
    reader(0, buf);

    struct elf32_header *header = (void*) buf;
    struct elf32_program_header *pheader = (void*)(buf + header->e_phoff);

    if (header->e_phnum != 1 ||
        header->e_phoff + header->e_phentsize > BLOCK_SIZE)
        FATAL("Grass exec region of the disk seems to be corrupted");

    if (pheader->p_vaddr == GRASS_ENTRY) {
        load_grass(reader, pheader);
    } else if (pheader->p_vaddr == APPS_ENTRY) {
        load_app(pid, reader, argc, argv, pheader);
    } else {
        FATAL("ELF gives invalid p_vaddr: 0x%.8x", pheader->p_vaddr);
    }
}

