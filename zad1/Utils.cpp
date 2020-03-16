#include <elf.h>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

#include "Utils.hpp"

void integrity_check(Elf64_Ehdr h) {
    if(h.e_ident[1] != 'E' ||
       h.e_ident[2] != 'L' ||
       h.e_ident[3] != 'F') {
        throw "Given file is not in ELF format!";
    }
}

/**
 * Reads ELF header from given file content.
 * Does integrity check, in case of error throws exception.
 **/
Elf64_Ehdr get_elf_header(const std::string& content) {
    Elf64_Ehdr header;
    
    std::memcpy(&header, content.c_str(), sizeof(Elf64_Ehdr));
    // cout << hex << setfill('0') << setw(2) << header.e_ident ;

    return header;
}


/**
 * Get address of exact program header offset.
 * Called with num=0 returns adress of program header table.
 **/
inline size_t get_ph_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_phoff + (num * hdr.e_phentsize);
}



/**
 * Returns vector of all program headers.
 **/
std::vector<Elf64_Phdr> get_phs(Elf64_Ehdr h, std::string content) {
    std::vector<Elf64_Phdr> res;
    for (int i = 0; i < h.e_phnum; i++) {
        Elf64_Phdr header;
        std::memcpy(&header, &content[get_ph_offset(h, i)], h.e_phentsize);
        res.push_back(header);
    }
    return res;
}

void print_program_header(Elf64_Phdr h) {
    if (h.p_type != PT_LOAD)
        return;
    std::cout << std::hex << "Header of type PT_LOAD, starting at file offset " << h.p_offset
        << " and size " << h.p_filesz << " or mem: " << h.p_memsz << ", mapping to "
        << h.p_vaddr << "\n";
}