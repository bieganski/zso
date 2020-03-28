#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>

#include "Utils.hpp"
#include "SectionEditor.hpp"

using namespace std;

typedef SectionEditor SE;

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

        size_t base = 0x800000;

        for (auto& pair : res) {
            Elf64_Shdr& hdr = pair.first;
            string& name = pair.second;
            hdr.sh_addr = base + off;
            hdr.sh_offset = off;
            hdr.sh_name = name_idx;

            name_idx++;
            off += hdr.sh_size;
        }

        return res;
}

int main() {
    auto input_pair = read_input_elfs("exec_syscall", "rel_syscall.o");
    
    std::string exec_content = input_pair.first;
    std::string rel_content = input_pair.second;


    Elf64_Ehdr exec_hdr;
    Elf64_Ehdr rel_hdr;

    try {
        exec_hdr = get_elf_header(exec_content);
    } catch (...) {
        std::cerr << "ET_EXEC input file is not in ELF format!\n";
        exit(1);
    }

    try {
        rel_hdr = get_elf_header(rel_content);
    } catch (...) {
        std::cerr << "ET_REL input file is not in ELF format!\n";
        exit(1);
    }

    integrity_check(exec_hdr);
    integrity_check(rel_hdr);

    std::vector<section_descr> section_hdrs = SE::get_shdrs(rel_content);

    std::vector<section_descr> sections_to_move;
    for (auto pair : section_hdrs) {
        Elf64_Shdr hdr = pair.first;
        if (hdr.sh_flags & SHF_ALLOC) {
            sections_to_move.push_back(pair);
            // SectionEditor::print_section(pair);
        }
    }

    std::vector<std::string> moved_sections_contents;

    for (auto& pair : sections_to_move) {
        Elf64_Shdr& hdr = pair.first;
        string& name = pair.second;

        moved_sections_contents.push_back(SE::get_section_content(rel_content, name));
    }

    auto name_positions = SE::add_moved_section_names(exec_content, sections_to_move);
    for (size_t& el : name_positions) {
        cout << dec << el << ",";
    }

    SE::append_sections(exec_content, sections_to_move, moved_sections_contents, name_positions);

    SE::dump(exec_content, "tescik");
}
