#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>

using namespace std;


using section_descr = std::pair<Elf64_Shdr, std::string> ;


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

    return header;
}

/**
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

std::string get_section_content(const std::string& content, Elf64_Shdr section_hdr) {
    Elf64_Ehdr h = get_elf_header(content);
    int begin = section_hdr.sh_offset;
    return content.substr(begin, section_hdr.sh_size);
}

/**
 * Returns vector of all sections headers with it's names.
 **/
 std::vector<section_descr> get_shdrs(Elf64_Ehdr h, std::string& content) {
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
    std::cout << std::hex << "Header of type PT_LOAD, starting at file offset " << h.p_offset
        << " and size " << h.p_filesz << " or mem: " << h.p_memsz << ", mapping to "
        << h.p_vaddr << "\n";
}

void print_section_header(Elf64_Shdr hdr) {
    std::cout << std::hex << "offset: " << hdr.sh_offset << "\nSize: " << hdr.sh_size << "\n";
}

void print_section(section_descr& tup) {
    Elf64_Shdr& hdr = tup.first;
    std::string& name = tup.second;
    std::cout << std::dec << "SECTION of name " << name << ":\n";
    print_section_header(hdr);
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

/**
 * Returns last number of byte that belongs to given section.
 **/ 
inline size_t section_end_offset(Elf64_Shdr hdr) {
    return hdr.sh_offset + hdr.sh_size;
}

size_t find_section_idx(std::string name, std::vector<section_descr> sections) {
    for (size_t i  = 0; i < sections.size(); i++) {
        if (sections[i].second == name)
            return i;
    }
    throw ("find_section: Cannot find section " + name + "\n");
}

Elf64_Shdr find_section(std::string name, std::vector<section_descr> sections) {
    return sections[find_section_idx(name, sections)].first;
}

inline void override_elf_hdr(std::string& content, Elf64_Ehdr hdr) {
    memcpy(content.data(), &hdr, hdr.e_ehsize);
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

    auto exec_s_hdrs = get_shdrs(exec_hdr, exec_content);
    auto rel_s_hdrs = get_shdrs(rel_hdr, rel_content);

    Elf64_Shdr rel_text_hdr = find_section(".text", rel_s_hdrs);
    std::string rel_text = get_section_content(rel_content, rel_text_hdr);
    
    std::string res = exec_content.insert(section_end_offset(rel_text_hdr), rel_text);
    Elf64_Shdr rel_new = rel_text_hdr;
    rel_new.
    override_elf_hdr(res, )
    // for (auto sh : rel_s_hdrs)
    //     print_section(sh);
    

    dump(res, "tescik");
}
