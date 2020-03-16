#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>

#include <csignal>


#include "Utils.hpp"
#include "SectionEditor.hpp"

std::string SectionEditor::get_section_content(Elf64_Shdr section_hdr) {
    Elf64_Ehdr h = get_elf_header(content);
    int begin = section_hdr.sh_offset;
    // char _res[section_hdr.sh_size];
    // memcpy(_res, content.data(), section_hdr.sh_size);
    // std::string res(_res, section_hdr.sh_size); // = content.substr(begin, section_hdr.sh_size);
    std::string res = content.substr(begin, section_hdr.sh_size);
    assert(res.size() == section_hdr.sh_size);
    return res;
}

// SectionEditor::SectionEditor(const std::string elf) : content(elf) {};


/**
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t SectionEditor::get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

std::string SectionEditor::get_section_content(const std::string& name) {
    std::vector<section_descr> s_tbl = get_shdrs();
    Elf64_Shdr shdr = find_section(name, s_tbl);
    return get_section_content(shdr);
}


/**
 * Returns vector of all sections headers with it's names.
 **/
std::vector<section_descr> SectionEditor::get_shdrs() {
    Elf64_Ehdr h = get_elf_header(content);
    std::vector<Elf64_Shdr> s_hdrs;
    for (int i = 0; i < h.e_shnum; i++) {
        Elf64_Shdr header;
        std::memcpy(&header, &content[get_sh_offset(h, i)], h.e_shentsize);
        s_hdrs.push_back(header);
    }

    std::vector<section_descr> res;
    
    std::string shstr_content = get_section_content(s_hdrs[h.e_shstrndx]);
    for (Elf64_Shdr s_hdr : s_hdrs) {
        // now, we have to be careful, because our 'content' string
        // contains null characters. We use C-functions to copy bytes 
        // only to first null character.
        res.push_back(std::make_pair(
            s_hdr, 
            std::string( &shstr_content.data()[s_hdr.sh_name] ))
        );
    }
    
    return res;
}


/**
 * Returns last number of byte that belongs to given section.
 **/ 
inline size_t SectionEditor::section_end_offset(Elf64_Shdr hdr) {
    return hdr.sh_offset + hdr.sh_size;
}


size_t SectionEditor::find_section_idx(const std::string& name, const std::vector<section_descr>& sections) {
    for (size_t i  = 0; i < sections.size(); i++) {
        if (sections[i].second == name)
            return i;
    }
    throw ("find_section: Cannot find section " + name + "\n");
}

Elf64_Shdr SectionEditor::find_section(const std::string& name, const std::vector<section_descr>& sections) {
    return sections[find_section_idx(name, sections)].first;
}


/**
 *  *Adds offset to all sections after `sec_name` and increase `sec_name`'s size
 * by `num` value.
 *  *Updates indexes in .shstrtab (it occurs as a last section).
 *  *Updates elf header's .shstrtab offset.
 **/
void SectionEditor::add_offset(const std::string& sec_name, size_t num) {
    
    std::vector<section_descr> sec_tbl = get_shdrs();
    Elf64_Shdr sec_hdr = find_section(sec_name, sec_tbl);

    size_t my_offset = sec_hdr.sh_offset;

    for (auto & sec_descr : sec_tbl) {
        if (sec_descr.second == sec_name) {
            // edited one, change it's size
            sec_descr.first.sh_size += num;
        }
        else if (sec_descr.first.sh_offset > my_offset) {
            // section is placed behind edited one
            // increase it's offset by number of bytes inserted
            sec_descr.first.sh_offset += num;
        }

        if (false) {
            // TODO, czy to może być przed sekcją .text? (pewnie tak!)
            sec_descr.first.sh_name += num;
        }
    }

    replace_sec_hdr_tbl(sec_tbl);

    // update elf header .shstrtab offset

    Elf64_Ehdr ehdr = get_elf_header(content);
    
    if (ehdr.e_phoff > my_offset)
        ehdr.e_phoff += num;
        
    if (ehdr.e_shoff > my_offset)
        ehdr.e_shoff += num;
  
    content.replace(0, ehdr.e_ehsize, (const char*) &ehdr, ehdr.e_ehsize);
    
}

/**
 * TODO na razie obsługuje ten sam size.
 */
void SectionEditor::replace_sec_hdr_tbl(std::vector<section_descr> new_tbl) {
    Elf64_Ehdr e_hdr = get_elf_header(content);
    
    assert(new_tbl.size() == e_hdr.e_shnum);

    size_t begin = e_hdr.e_shoff;
    size_t ent_size = e_hdr.e_shentsize;
    
    for (size_t i = 0; i < new_tbl.size(); i++) {
        size_t addr = begin + i * ent_size;
        std::cout << std::hex << "changing off: " << addr << " of " << ent_size <<"bytes \n";
        size_t s1 = content.size();
        content.replace(addr, ent_size, (const char*) &new_tbl[i].first, ent_size);
        size_t s2 = content.size();
        assert(s1 == s2);
    }
}

    /**
     * Inserts `what` string to end of `sec_name` section.
     * Returns std::string that represents changed binary.
     */
void SectionEditor::insert_to_section(const std::string& what, const std::string& sec_name) {
    
    size_t begin_len = content.size();

    std::vector<section_descr> init_sec_tbl = get_shdrs();
    Elf64_Shdr sec_hdr = find_section(sec_name, init_sec_tbl);

    size_t insert_before_idx = section_end_offset(sec_hdr) + 1;

// #define ELF_ASSERTS 1
// #ifdef ELF_ASSERTS
//     size_t lo_sec_addr = SIZE_MAX;
//     for (auto s_hdr : init_sec_tbl) {
//         lo_sec_addr = std::min(lo_sec_addr, s_hdr.first.sh_offset);
//     }
//     assert(get_elf_header(content).e_shoff < lo_sec_addr);
// #endif
    

    // for(int i = 0; i < init_sec_tbl.size(); i++) {
    //     std::cout << "---- NOWA\n";
    //     print_section(init_sec_tbl[i]);
    //     std::cout << "---- STARA\n";
    //     print_section(new_sect_tbl[i]);
    //     std::cout << "-------------------\n";
    // }

    // TODO - logical error, `content` should be const (initial one) (or maybe no?)

    add_offset(sec_name, what.size());
    content.insert(insert_before_idx, what.data(), what.size());


    // std::vector<section_descr> aaa = get_shdrs();

    // for(int i = 0; i < aaa.size(); i++) {
    //     std::cout << "---- NOWA\n";
    //     print_section(aaa[i]);
    // }


    size_t end_len = content.size();

    // TODO assert(begin_len + what.size() == end_len);
}

void SectionEditor::dump(std::string out_file_path) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
}
