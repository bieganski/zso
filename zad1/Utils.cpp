#include <elf.h>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <map>

#include "Utils.hpp"
#include "SectionEditor.hpp"

typedef SectionEditor SE;

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
    
    std::memcpy(&header, content.data(), sizeof(Elf64_Ehdr));
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
std::vector<Elf64_Phdr> get_phs(const std::string& content) {
    Elf64_Ehdr h = get_elf_header(content);
    std::vector<Elf64_Phdr> res;
    for (int i = 0; i < h.e_phnum; i++) {
        Elf64_Phdr header;
        std::memcpy(&header, &content[get_ph_offset(h, i)], h.e_phentsize);
        res.push_back(header);
    }
    return res;
}

void replace_pdhr_tbl(std::string& content, std::vector<Elf64_Phdr> new_tbl) {
    Elf64_Ehdr ehdr = get_elf_header(content);
    size_t tbl_off = ehdr.e_phoff;
    size_t tbl_size = ehdr.e_phentsize * ehdr.e_phnum;

assert(ehdr.e_phnum == new_tbl.size()); // TODO
    std::string res;
    for (Elf64_Phdr entry : new_tbl) {
        res.append((const char*) &entry, ehdr.e_phentsize);
    }

    content.replace(tbl_off, tbl_size, res.data(), res.size());
}

void print_program_header(Elf64_Phdr h) {
    if (h.p_type != PT_LOAD)
        return;
    std::cout << std::hex << "Header of type PT_LOAD, starting at file offset " << h.p_offset
        << " and size " << h.p_filesz << " or mem: " << h.p_memsz << ", mapping to "
        << h.p_vaddr << "\n";
}

std::map<Elf64_Phdr, std::vector<Elf64_Shdr>, DataComparer<Elf64_Phdr>> sec2seg_map(const std::string& content) {

    std::map<Elf64_Phdr, std::vector<Elf64_Shdr>, DataComparer<Elf64_Phdr>> res;
    auto s_hdrs = SE::get_shdrs(content);
    std::vector<Elf64_Phdr> p_hdrs = get_phs(content);

    for (Elf64_Phdr p : p_hdrs) {
        auto pair = res.insert(std::make_pair(p, std::vector<Elf64_Shdr>{}));
        auto it = pair.first;
        for (auto ppair : s_hdrs) {
            Elf64_Shdr s = ppair.first;
            if (s.sh_offset >= p.p_offset && s.sh_offset <= p.p_offset + p.p_filesz) {
                it->second.push_back(s);
            }
        }
    }

    // std::cout << "MAPA ==   == = = = == = = == =:\n";
    // for(auto it = res.begin(); it != res.end(); ++it)
    // {
    //     print_program_header(it->first);
    //     for (auto el : it->second) {
    //         se.print_section_header(el);
    //     }
        
    // }

    return res;
}
