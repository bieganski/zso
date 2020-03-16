#include <elf.h>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

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
std::vector<Elf64_Phdr> get_phs(Elf64_Ehdr h, std::string content);


void print_program_header(Elf64_Phdr h);