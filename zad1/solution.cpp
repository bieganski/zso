#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>

#include "Utils.hpp"
#include "SectionEditor.hpp"

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


// TODO pozbyc sie
void dump(std::string out_file_path, std::string& content) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
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

    // SectionEditor exec_s_ed(exec_content);

    // exec_s_ed.insert_to_section(rel_text, ".text");

    // -----------------------------------

    size_t __pos0 = exec_content.size();
    auto phs = get_phs(exec_content);

    // TODO sprawdzic referencje
    for (auto &ph : phs) {
        if (ph.p_type == PT_PHDR) {
            ph.p_offset = __pos0 + 64;
            ph.p_vaddr += 0x2030;
            ph.p_paddr += 0x2030;
        }
        if (ph.p_offset == 0) {
            assert(ph.p_type == PT_LOAD);
            ph.p_offset = __pos0;
            ph.p_vaddr += 0x2030;
            ph.p_paddr += 0x2030;
            break;
        }
    }

    replace_pdhr_tbl(exec_content, phs);

    Elf64_Ehdr ehdr = get_elf_header(exec_content);

    ehdr.e_entry += 0x2030;
    ehdr.e_phoff = __pos0 + ehdr.e_ehsize;

    exec_content.replace(0, ehdr.e_ehsize, (const char*) &ehdr, ehdr.e_ehsize);

    SectionEditor exec_s_ed(exec_content);

    std::string ehphs = exec_content.substr(0, 0x6e0);

    exec_s_ed.append(ehphs);

    auto totalres = exec_s_ed.get_content();

    dump("tescik", totalres);
    // // ----------------------------------

    // // EHDR + PHDR = 120 bajtow
    // std::string res = exec_content.substr(0, 0x6e0);
    // size_t __pos0 = exec_content.size;
    // size_t pos0 = exec_s_ed.append(res);
    // auto phs = get_phs(exec_content);

    // // TODO sprawdzic referencje
    // for (auto &ph : phs) {
    //     if (ph.p_offset == 0) {
    //         assert(ph.p_type == PT_LOAD);
    //         ph.p_offset = pos0;
    //         break;
    //     }
    // }

    // std::string res_content = exec_s_ed.get_content();
    // replace_pdhr_tbl(res_content, phs);

    // Elf64_Ehdr ehdr = get_elf_header(res_content);
    // ehdr.e_phoff = pos0 + ehdr.e_ehsize;
    // res_content.replace(0, ehdr.e_ehsize, (const char*) &ehdr, ehdr.e_ehsize);

    // dump("tescik", res_content);

    // exec_s_ed.dump("tescik");
}



// int main() {

//     string a{"123456", 5};
//     char aa[] = "k\0u";
//     cout << a << "\n";
//     a.replace(2, 3, aa, 3);
//     cout << a << a.size();
// }