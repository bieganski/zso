#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>

using namespace std;

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
    
    integrity_check(header);

    return header;
}

/**
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

/**
 * Returns vector of all sections headers.
 **/
std::vector<Elf64_Shdr> get_shs(Elf64_Ehdr h, std::string content) {
    std::vector<Elf64_Shdr> res;
    for (int i = 0; i < h.e_shnum; i++) {
        Elf64_Shdr header;
        std::memcpy(&header, &content[get_sh_offset(h, i)], h.e_shentsize);
        cout << "SS:" << header.sh_size << "\n";
        res.push_back(header);
    }
    return res;
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



void dump(std::string content, std::string out_file_path) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
}


void print_program_header(Elf64_Phdr h) {
    if (h.p_type != PT_LOAD)
        return;
    cout << std::hex << "Header of type PT_LOAD, starting at file offset " << h.p_offset
        << " and size " << h.p_filesz << " or mem: " << h.p_memsz << ", mapping to "
        << h.p_vaddr << "\n";
}


std::string get_section_content(const std::string& content, Elf64_Shdr section_hdr) {
    Elf64_Ehdr h = get_elf_header(content);
    int begin = section_hdr.sh_offset;
    return content.substr(begin, begin + section_hdr.sh_size);

    // TODO null
}

void print_section_header(Elf64_Shdr h) {
    cout << std::dec << "idx:" << h.sh_name << "\n";
}

void offsetSections(Elf64_Ehdr hdr, size_t offset) {
    //      zmiany w Elf64_Shdr:
    // Elf64_Off	sh_offset;		/* Section file offset */
    // Elf64_Word	sh_link;		/* Link to another section */
    
    // hdr.
}

void addHeader(Elf64_Phdr h, std::string& content, const std::string& what) {
    Elf64_Ehdr hdr = get_elf_header(content);
    content.insert(hdr.e_phoff + (hdr.e_phnum * hdr.e_phentsize), what);

    // now, all offsets must be updated

}

int main() {
    ifstream exec_bin{"exec_syscall"};

    std::string exec_content((std::istreambuf_iterator<char>(exec_bin)),
                              std::istreambuf_iterator<char>());


    Elf64_Ehdr header = get_elf_header(exec_content);

    // auto p_hdrs = get_phs(header, exec_content);

    // for (auto ph : p_hdrs)
    //     print_program_header(ph);

    auto s_hdrs = get_shs(header, exec_content);

    // for (auto sh : s_hdrs)
    //     print_section_header(sh);

        

    // dump(exec_content, "tescik");
}




// int main() {

//     string a {"aa\0bb"};
//     cout << a.size();
// }