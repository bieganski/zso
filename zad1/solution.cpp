#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>
#include <unistd.h>

#include "Utils.hpp"
#include "SectionEditor.hpp"

using namespace std;

typedef SectionEditor SE;

const static size_t BASE_REL = 0x800000;

const static u_int64_t EXEC_BASE = 0x400000;

std::pair<std::string, std::string> read_input_elfs(std::string exec_fname, std::string rel_fname) {

    try {
        
        std::ifstream exec_bin{exec_fname};
        std::string exec_content((std::istreambuf_iterator<char>(exec_bin)),
                                std::istreambuf_iterator<char>());

                                
        std::ifstream rel_bin{rel_fname};
        std::string rel_content((std::istreambuf_iterator<char>(rel_bin)),
                                std::istreambuf_iterator<char>());

        exec_bin.close();
        rel_bin.close();

        return std::make_pair(exec_content, rel_content);
    } catch (...) {
        std::cerr << "ERROR: Cannot open given file!\n";
        exit(1);
    }
}

std::vector<section_descr> update_section_hdrs_move(const std::vector<section_descr>& sections_to_move, 
    size_t off,
    size_t name_idx) {

        std::vector<section_descr> res(sections_to_move);

        for (auto& pair : res) {
            Elf64_Shdr& hdr = pair.first;
            string& name = pair.second;
            hdr.sh_addr = BASE_REL + off;
            hdr.sh_offset = off;
            hdr.sh_name = name_idx;

            name_idx++;
            off += hdr.sh_size;
        }

        return res;
}

size_t compute_num_additional_pages(size_t num_new_load_sections, size_t phentsize) {
    size_t additional_bytes = num_new_load_sections * phentsize;
    size_t num_pages = additional_bytes / getpagesize() + 1;
    assert (num_pages == 1); // TODO wywalić
    return num_pages;
}

Elf64_Word get_phflags(Elf64_Xword sh_flags) {
    Elf64_Word flags = 0;

    flags |= PF_R;

    if (sh_flags & SHF_WRITE) {
        flags |= PF_W;
    }

    if (sh_flags & SHF_EXECINSTR) {
        flags |= PF_X;
    }

    return flags;
}

void resolve_relocations(std::string&, const std::string&);

int main() {
    auto input_pair = read_input_elfs("exec_syscall", "rel_syscall.o");
    
    std::string exec_content = input_pair.first;
    std::string rel_content = input_pair.second;

    try {
        integrity_check(get_elf_header(exec_content));
    } catch (...) {
        std::cerr << "ET_EXEC input file is not in ELF format!\n";
        exit(1);
    }

    try {
        integrity_check(get_elf_header(rel_content));
    } catch (...) {
        std::cerr << "ET_REL input file is not in ELF format!\n";
        exit(1);
    }


    std::vector<section_descr> section_hdrs = SE::get_shdrs(rel_content);

    std::vector<section_descr> sections_to_move;
    for (auto pair : section_hdrs) {
        Elf64_Shdr hdr = pair.first;
        if (hdr.sh_flags & SHF_ALLOC) {
            sections_to_move.push_back(pair);
        }
    }

    std::vector<std::string> moved_sections_contents;

    for (auto& pair : sections_to_move) {
        Elf64_Shdr& hdr = pair.first;
        string& name = pair.second;

        moved_sections_contents.push_back(SE::get_section_content(rel_content, name));
    }

    auto name_positions = SE::add_moved_section_names(exec_content, sections_to_move);

    SE::append_sections(exec_content, sections_to_move, moved_sections_contents, name_positions);

    // now, we will be inserting place for new program headers

    Elf64_Ehdr exec_hdr = get_elf_header(exec_content);

    size_t num_new_phdrs = 1 + sections_to_move.size(); // '1' for additional PT_LOAD with new headers
    size_t num_pages_begin = compute_num_additional_pages(num_new_phdrs, exec_hdr.e_phentsize);
    size_t whole_size = getpagesize() * num_pages_begin;
    std::string begin_buf;

    // new elf header and program headers
    // TODO increase all offsets (both PHdrs, SHdrs)

    exec_hdr.e_shoff += whole_size;
    exec_hdr.e_phoff = exec_hdr.e_ehsize; // just behind elf header
    exec_hdr.e_phnum += num_new_phdrs;

    begin_buf.append((const char*) &exec_hdr, sizeof(Elf64_Ehdr));
    auto phdrs = get_phs(exec_content);

    // first PT_LOAD segment maps elf header and program headers.
    // create it manually.
    Elf64_Phdr first_load {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = 0,
        .p_vaddr = EXEC_BASE - whole_size,
        .p_paddr = EXEC_BASE - whole_size,
        .p_filesz = exec_hdr.e_ehsize + (num_new_phdrs + phdrs.size()) * exec_hdr.e_phentsize,
        .p_memsz = exec_hdr.e_ehsize + (num_new_phdrs + phdrs.size()) * exec_hdr.e_phentsize,
        .p_align = (Elf64_Xword) 1,
    };

    for (auto& ph : phdrs) {
        if (ph.p_type == PT_PHDR) {
            size_t addr = EXEC_BASE - whole_size + exec_hdr.e_phoff;
            ph.p_paddr = addr;
            ph.p_vaddr = addr;
        } else {
            ph.p_offset += whole_size;
        }   
    }

    // phdrs.insert(phdrs.begin(), first_load);

    // generate new program headers and add offsets to existing program headers table
    std::vector<Elf64_Phdr> new_phdrs;
    for (const auto& pair: sections_to_move) {
        Elf64_Shdr shdr = pair.first;
        Elf64_Phdr phdr {
            .p_type = PT_LOAD,
            .p_flags = get_phflags(shdr.sh_flags),
            .p_offset = whole_size + shdr.sh_offset,
            .p_vaddr = BASE_REL + shdr.sh_offset,
            .p_paddr = BASE_REL + shdr.sh_offset,
            .p_filesz = shdr.sh_type & SHT_NOBITS ? 0 : shdr.sh_size,
            .p_memsz = shdr.sh_size,
            .p_align = (Elf64_Xword) getpagesize(),
        };
        new_phdrs.push_back(phdr);
    }

    phdrs.insert(phdrs.end(), new_phdrs.begin(), new_phdrs.end());
    for (size_t i = 0; i < 2; i++) {
        size_t addr = exec_hdr.e_ehsize + i * exec_hdr.e_phentsize;
        begin_buf.append((const char*) &phdrs[i], sizeof(Elf64_Phdr));
    }
    begin_buf.append((const char*) &first_load, sizeof(Elf64_Phdr));
    for (size_t i = 2; i < phdrs.size(); i++) {
        size_t addr = exec_hdr.e_ehsize + i * exec_hdr.e_phentsize;
        begin_buf.append((const char*) &phdrs[i], sizeof(Elf64_Phdr));
    }


    // Add offsets to section headers table
    auto shdrs = SE::get_shdrs(exec_content);
    for (auto &pair : shdrs) {
        Elf64_Shdr& shdr = pair.first;
        shdr.sh_offset += whole_size;
    }
    SE::replace_sec_hdr_tbl(exec_content, shdrs);

    begin_buf.resize(whole_size, '\0');
    exec_content.insert(0, begin_buf);

    resolve_relocations(exec_content, rel_content);

    SE::dump(exec_content, "tescik");
}

using symtab_descr = std::pair<Elf64_Sym, std::string>;

std::vector<section_descr> get_rela_sections(const std::string& content) {
    auto pairs = SE::get_shdrs(content);
    std::vector<section_descr> res;
    for (auto& pair : pairs) {
        if (pair.first.sh_type == SHT_RELA)
            res.push_back(pair);
    }
    assert(res.size() > 0);
    return res;
}


std::vector<symtab_descr> get_symbols(const std::string& content) {
    auto shdrs = SE::get_shdrs(content);
    Elf64_Shdr symtab = SE::find_section(".symtab", shdrs);
    std::string strtab_content = SE::get_section_content(content, ".strtab");
    std::vector<symtab_descr> res;

    assert(symtab.sh_size % sizeof(Elf64_Sym) == 0);

    for (size_t i = 0; i < symtab.sh_size; i = i + sizeof(Elf64_Sym)) {
        Elf64_Sym sym;
        size_t addr = i + symtab.sh_offset;
        memcpy(&sym, &content.data()[addr], sizeof(Elf64_Sym));
        std::string s(&strtab_content.data()[sym.st_name]); // to first null char
        res.push_back(std::make_pair(sym, s));
    }

    return res;
}

void resolve_relocations(std::string& exec_content, const std::string& rel_content) {
    auto symbols = get_symbols(exec_content);
    auto relas = get_rela_sections(exec_content);

    return;
}