#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>


using section_descr = std::pair<Elf64_Shdr, std::string> ;

extern size_t BASE_REL;

// `content` argument always means elf file content 
class SectionEditor {
private:
    static std::string get_section_content(const std::string& content, Elf64_Shdr section_hdr);

public:

    static void print_section_header(Elf64_Shdr hdr) {
        std::cout << std::hex << "offset: " << hdr.sh_offset << "\nSize: " << hdr.sh_size << "\n";
    }

    static void print_section(section_descr& tup) {
        Elf64_Shdr& hdr = tup.first;
        std::string& name = tup.second;
        std::cout << std::dec << "SECTION of name " << name << ":\n";
        std::cout << std::dec << "SECTION of name_idx " << hdr.sh_name << ":\n";
        print_section_header(hdr);
    }

    /**
     * Get address of exact section header offset.
     * Called with num=0 returns adress of section header table.
     **/
    inline static size_t get_sh_offset(Elf64_Ehdr hdr, size_t num);

    static std::string get_section_content(const std::string& content, const std::string& name);

    /**
     * Returns vector of all sections headers with it's names.
     **/
    static std::vector<section_descr> get_shdrs(const std::string& content);

    /**
     * Returns last number of byte that belongs to given section.
     **/ 
    inline static size_t section_end_offset(Elf64_Shdr hdr);

    static size_t find_section_idx(const std::string& name, const std::vector<section_descr>& sections);

    static Elf64_Shdr find_section(const std::string& name, const std::vector<section_descr>& sections);

    static void add_offset(std::string& content, const std::string& sec_name, size_t num);

    static void append_sections(std::string& content, 
                                    std::vector<section_descr>& new_sections, 
                                    const std::vector<std::string>& new_sections_contents,
                                    const std::vector<size_t>& names_positions);
    /**
     * Inserts `what` string to end of `sec_name` section.
     * Returns position of insertion beginning.
     */
    static size_t insert_to_section(std::string& content, const std::string& what, const std::string& sec_name);

    /**
     * Inserts 'what' string to `pos` byte.
     * If `pos` == 0, then it inserts it to section begin,
     * if `pos` == section_size, then to section end.
     * Returns position of insertion beginning.
     **/
    static size_t insert_to_section_pos(std::string& content, const std::string& what, const std::string& sec_name, size_t pos);

    /**
     * Returns position of first byte added.
     */
    static size_t append(std::string& content, const std::string& what);

    static void dump(const std::string& content, const std::string& out_file_path);

    /**
     * Returns vector of positions of inserted section names.
     */
    static std::vector<size_t> add_moved_section_names(std::string&, std::vector<section_descr>&, const std::string& prefix);

    static void replace_sec_hdr_tbl(std::string& content, std::vector<section_descr>& new_tbl);

    static size_t get_section_vaddr(const std::string& content, const std::string& name);

    static size_t get_section_idx(const std::string& content, const std::string& name);
};