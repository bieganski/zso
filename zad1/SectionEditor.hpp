#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>


using section_descr = std::pair<Elf64_Shdr, std::string> ;


class SectionEditor {
private:
    std::string content; // content of given elf

    std::string get_section_content(Elf64_Shdr section_hdr);

public:

    SectionEditor(const std::string elf) : content(elf) {};

    static void print_section_header(Elf64_Shdr hdr) {
        std::cout << std::hex << "offset: " << hdr.sh_offset << "\nSize: " << hdr.sh_size << "\n";
    }

    static void print_section(section_descr& tup) {
        Elf64_Shdr& hdr = tup.first;
        std::string& name = tup.second;
        std::cout << std::dec << "SECTION of name " << name << ":\n";
        print_section_header(hdr);
    }

    /**
     * Get address of exact section header offset.
     * Called with num=0 returns adress of section header table.
     **/
    inline size_t get_sh_offset(Elf64_Ehdr hdr, size_t num);

    std::string get_section_content(const std::string& name);

    /**
     * Returns vector of all sections headers with it's names.
     **/
    std::vector<section_descr> get_shdrs();

    /**
     * Returns last number of byte that belongs to given section.
     **/ 
    inline size_t section_end_offset(Elf64_Shdr hdr);

    size_t find_section_idx(const std::string& name, const std::vector<section_descr>& sections);

    Elf64_Shdr find_section(const std::string& name, const std::vector<section_descr>& sections);

    /**
     * Adds offset to all sections after `sec_name` and increase `sec_name`'s size
     * by `num` value. 
     **/
    std::vector<section_descr> add_offset(const std::string& sec_name, size_t num);
    /**
     * TODO na razie obsługuje ten sam size.
     */
    void replace_sec_hdr_tbl(std::vector<section_descr> new_tbl);

    /**
     * Inserts `what` string to end of `sec_name` section.
     * Returns std::string that represents changed binary.
     */
    void insert_to_section(const std::string& what, const std::string& sec_name);

    void dump(std::string out_file_path);
};