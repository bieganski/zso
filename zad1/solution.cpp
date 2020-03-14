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
Elf64_Ehdr getElfHeader(const std::string& content) {
    Elf64_Ehdr header;
    
    std::memcpy(&header, content.c_str(), sizeof(Elf64_Ehdr));
    // cout << hex << setfill('0') << setw(2) << header.e_ident ;
    
    integrity_check(header);

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
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

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


void printHeader(Elf64_Phdr h) {
    if (h.p_type != PT_LOAD)
        return;
    cout << hex << "Header of type PT_LOAD, starting at file offset " << h.p_offset
        << " and size " << h.p_filesz << " or mem: " << h.p_memsz << ", mapping to "
        << h.p_vaddr << "\n";
}

void offsetSections(Elf64_Ehdr hdr, size_t offset) {
    //      zmiany w Elf64_Shdr:
    // Elf64_Off	sh_offset;		/* Section file offset */
    // Elf64_Word	sh_link;		/* Link to another section */
    
    hdr.
}

void addHeader(Elf64_Phdr h, std::string content, const std::string what) {
    Elf64_Ehdr hdr = getElfHeader(content);
    content.insert(hdr.e_phoff + (hdr.e_phnum * hdr.e_phentsize), what);

    // now, all offsets must be updated

}

int main() {
    ifstream exec_bin{"exec_syscall"};

    std::string exec_content((std::istreambuf_iterator<char>(exec_bin)),
                              std::istreambuf_iterator<char>());
    
    Elf64_Ehdr header = getElfHeader(exec_content);
    auto p_hs = get_phs(header, exec_content);

    for (auto ph : p_hs)
        printHeader(ph);    
    // dump(exec_content, "tescik");
}