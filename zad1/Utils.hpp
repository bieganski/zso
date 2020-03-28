#include <elf.h>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <map>

void integrity_check(Elf64_Ehdr h);

/**
 * Reads ELF header from given file content.
 * Does integrity check, in case of error throws exception.
 **/
Elf64_Ehdr get_elf_header(const std::string& content);


/**
 * Get address of exact program header offset.
 * Called with num=0 returns adress of program header table.
 **/
inline size_t get_ph_offset(Elf64_Ehdr hdr, size_t num);

/**
 * Returns vector of all program headers.
 **/
std::vector<Elf64_Phdr> get_phs(const std::string& content);


void print_program_header(Elf64_Phdr h);


template<typename T>
struct DataComparer {
    bool operator()(const T lhs, const T rhs) const {
        int cmp = memcmp(&lhs, &rhs, sizeof(lhs));
        return cmp; // cmp < 0 || cmp == 0 && lhs.len < rhs.len;
    }
};

std::map<Elf64_Phdr, std::vector<Elf64_Shdr>, DataComparer<Elf64_Phdr>> sec2seg_map(const std::string& content);


void replace_pdhr_tbl(std::string& content, std::vector<Elf64_Phdr> new_tbl);