#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>

#include <csignal>
#include <stdlib.h>

#include "Utils.hpp"
#include "SectionEditor.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

#include <stddef.h>


typedef SectionEditor SE;

std::string SE::get_section_content(const std::string& content, Elf64_Shdr section_hdr) {
    Elf64_Ehdr h = get_elf_header(content);
    int begin = section_hdr.sh_offset;
    // char _res[section_hdr.sh_size];
    // memcpy(_res, content.data(), section_hdr.sh_size);
    // std::string res(_res, section_hdr.sh_size); // = content.substr(begin, section_hdr.sh_size);
    std::string res = content.substr(begin, section_hdr.sh_size);
    assert(res.size() == section_hdr.sh_size);
    return res;
}


/**
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t SE::get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

std::string SE::get_section_content(const std::string& content, const std::string& name) {
    std::vector<section_descr> s_tbl = get_shdrs(content);
    Elf64_Shdr shdr = find_section(name, s_tbl);
    return get_section_content(content, shdr);
}


/**
 * Returns vector of all sections headers with it's names.
 **/
std::vector<section_descr> SE::get_shdrs(const std::string& content) {
    Elf64_Ehdr h = get_elf_header(content);
    std::vector<Elf64_Shdr> s_hdrs;
    for (int i = 0; i < h.e_shnum; i++) {
        Elf64_Shdr header;
        std::memcpy(&header, &content[get_sh_offset(h, i)], h.e_shentsize);
        s_hdrs.push_back(header);
    }

    std::vector<section_descr> res;
    
    std::string shstr_content = get_section_content(content, s_hdrs[h.e_shstrndx]);

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

bool sec_hdr_tbl_at_very_end(const std::string& content) {
    Elf64_Ehdr ehdr = get_elf_header(content);
    bool res = ehdr.e_shoff + ehdr.e_shnum * ehdr.e_shentsize == content.size();
    return res;
}

/**
 * Returns last number of byte that belongs to given section.
 **/ 
inline size_t SE::section_end_offset(Elf64_Shdr hdr) {
    return hdr.sh_offset + hdr.sh_size;
}

size_t SE::find_section_idx(const std::string& name, const std::vector<section_descr>& sections) {
    for (size_t i  = 0; i < sections.size(); i++) {
        if (sections[i].second == name)
            return i;
    }
    throw ("find_section: Cannot find section " + name + "\n");
}


size_t SE::get_section_idx(const std::string& content, const std::string& name) {
    auto shdrs = SE::get_shdrs(content);
    return SE::find_section_idx(name, shdrs);
}


Elf64_Shdr SE::find_section(const std::string& name, const std::vector<section_descr>& sections) {
    return sections[find_section_idx(name, sections)].first;
}

/**
 * Used for finding ET_EXEC's mapped to virtual memory sections, thus
 * assert with nonzero return.
 */
size_t SE::get_section_vaddr(const std::string& content, const std::string& name) {
    auto shdrs = SE::get_shdrs(content);
    auto shdr = SE::find_section(name, shdrs);
    assert(shdr.sh_addr != 0);
    return shdr.sh_addr;
}


void SectionEditor::append_sections(std::string& content, 
                                    std::vector<section_descr>& new_sections, 
                                    const std::vector<std::string>& new_sections_contents) {
    if (!sec_hdr_tbl_at_very_end(content)) {
        Elf64_Ehdr ehdr = get_elf_header(content);
        std::string sec_hdr_table = content.substr(ehdr.e_shoff, ehdr.e_shentsize * ehdr.e_shnum);
        size_t pos0 = content.size(); 
        content.append(sec_hdr_table);
        ehdr.e_shoff = pos0;
        content.replace(0, sizeof(Elf64_Ehdr), (const char *) &ehdr, sizeof(Elf64_Ehdr));
    }
    assert(sec_hdr_tbl_at_very_end(content));
    SE::append_sections_help(content, new_sections, new_sections_contents);
}

// ASSUMPTION: section header table is at the very end of file
// TODO secitons align
void SectionEditor::append_sections_help(std::string& content, 
                                    std::vector<section_descr>& new_sections, 
                                    const std::vector<std::string>& new_sections_contents) {
    Elf64_Ehdr e_hdr = get_elf_header(content);

    assert(new_sections_contents.size() == new_sections.size());
    assert(content.size() == e_hdr.e_shoff + e_hdr.e_shnum * e_hdr.e_shentsize);

    auto actual_headers = SE::get_shdrs(content);
    
    content.erase(e_hdr.e_shoff, e_hdr.e_shnum * e_hdr.e_shentsize);

    assert(content.size() == e_hdr.e_shoff);
    size_t pos0 = content.size();
    
    for (size_t i = 0; i < new_sections.size(); i++) {
        content.append(new_sections_contents[i]);
    }

    e_hdr.e_shnum += new_sections.size();
    e_hdr.e_shoff = content.size();

    for (int i = 0; i < actual_headers.size(); i++) {
        content.append((const char*) &actual_headers[i], e_hdr.e_shentsize);
    }

    size_t off = pos0;

    for (size_t i = 0; i < new_sections.size(); i++) {
        Elf64_Shdr& hdr = new_sections[i].first;
        hdr.sh_addr = BASE_REL + off + i * 0x200000;
        hdr.sh_offset = off;
        hdr.sh_name = 0; // that value will be fullfilled by `add_moved_section_names` function

        off += new_sections_contents[i].size();
    }

    for (int i = 0; i < new_sections.size(); i++) {
        content.append((const char*) &new_sections[i], e_hdr.e_shentsize);
    }

    // update elf header
    content.replace(0, sizeof(Elf64_Ehdr), (const char*) &e_hdr, sizeof(Elf64_Ehdr));
}


void SectionEditor::dump(const std::string& content, const std::string& out_file_path) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
    // chmod(out_file_path.data(), 755);
    std::string s("chmod 755 ");
    s.append(out_file_path);
    system(s.data());
}

size_t SectionEditor::append(std::string& content, const std::string& what) {
    size_t init_size = content.size();
    content.append(what);
    assert(init_size + what.size() == content.size());
    return init_size;
}

inline size_t get_section_offset(const std::string& content, const std::string sec_name) {
    return SE::find_section(sec_name, SE::get_shdrs(content)).sh_offset;
}


/**
 * ASSUMPTION:
 * ".shstrtab" is the last section, after it is only section header table.
 *  Returns vector of positions relevant to .shstrtab offset.
 */


size_t section_insertion_addr(const std::string& content, Elf64_Shdr to_be_appended) {
    const size_t align = to_be_appended.sh_addralign;
    size_t proper_addr = content.size();
    for(uint i = 0; i <= align; i++) {
        if (proper_addr % align ==0)
            return proper_addr;
        proper_addr++;
    }
}

void SectionEditor::add_moved_section_names(std::string& content, 
                                                           std::vector<section_descr>& sections_to_move, 
                                                           const std::string& prefix) {
    std::vector<size_t> res;

    size_t s0 = content.size();
    Elf64_Shdr shstrtab_hdr = SE::find_section(".shstrtab", SE::get_shdrs(content));
    size_t shstrtab_off = shstrtab_hdr.sh_offset;
    size_t shstrtab_size = shstrtab_hdr.sh_size;
    Elf64_Ehdr ehdr = get_elf_header(content);

    std::string shstrtab_content = content.substr(shstrtab_off, shstrtab_size);

    content.replace(shstrtab_off, shstrtab_size, shstrtab_size, '\0'); // we want use it anymore

    size_t shstrtab_new_offset = section_insertion_addr(content, shstrtab_hdr);
    content.append(shstrtab_new_offset - content.size(), '\0');
    assert(content.size() == shstrtab_new_offset);
    content.append(shstrtab_content);

    size_t sum_size = 0;
    for (auto& pair : sections_to_move) {
        std::string& name = pair.second;

        std::string new_name = prefix + name;

        size_t name_pos = content.size() - shstrtab_new_offset;
        
        res.push_back(name_pos);
        content.append(new_name.data(), new_name.size());

        content.append("\0", 1);
        sum_size += new_name.size() + 1;
    }

    size_t shoff = SE::get_sh_offset(ehdr, ehdr.e_shstrndx);
    Elf64_Shdr tmp;
    std::memcpy(&tmp, &content[shoff], sizeof(Elf64_Shdr));
    tmp.sh_offset = shstrtab_new_offset;
    tmp.sh_size += sum_size;
    content.replace(shoff, sizeof(Elf64_Shdr), (const char *)&tmp, sizeof(Elf64_Shdr));

    auto sec_hdrs = SE::get_shdrs(content);

    auto j = 0;
    for (uint i = sec_hdrs.size() - sections_to_move.size(); i < sec_hdrs.size(); i++) {
        sec_hdrs[i].first.sh_name = res[j];
        j++;
    }

    replace_sec_hdr_tbl(content, sec_hdrs);
}


void SectionEditor::replace_sec_hdr_tbl(std::string& content, std::vector<section_descr>& new_tbl) {
    Elf64_Ehdr e_hdr = get_elf_header(content);
    
    assert(new_tbl.size() == e_hdr.e_shnum);
    size_t begin = e_hdr.e_shoff;
    size_t ent_size = e_hdr.e_shentsize;
    
    for (size_t i = 0; i < new_tbl.size(); i++) {
        size_t addr = begin + i * ent_size;
        size_t s1 = content.size();
        content.replace(addr, ent_size, (const char*) &new_tbl[i].first, ent_size);
        size_t s2 = content.size();
        assert(s1 == s2);
    }
}