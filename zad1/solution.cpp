#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>


#include "Utils.cpp"
#include "SectionEditor.cpp"

using namespace std;


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

    // auto p_hdrs = get_phs(header, exec_content);

    // for (auto ph : p_hdrs)
    //     print_program_header(ph);

    SectionEditor rel_s_ed(rel_content);

    std::string rel_text = rel_s_ed.get_section_content(".text");

    SectionEditor exec_s_ed(exec_content);
    exec_s_ed.insert_to_section(rel_text, ".text");

    exec_s_ed.dump("tescik");
}


