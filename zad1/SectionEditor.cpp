#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>

#include "Utils.cpp"


using section_descr = std::pair<Elf64_Shdr, std::string> ;


class SectionEditor {
private:
    std::string content; // content of given elf

    std::string get_section_content(Elf64_Shdr section_hdr) {
        Elf64_Ehdr h = get_elf_header(content);
        int begin = section_hdr.sh_offset;
        return content.substr(begin, section_hdr.sh_size);
    }

public:
    
    SectionEditor(const std::string elf) : content(elf) {};


    std::string get_section_content(Elf64_Shdr section_hdr) {
        Elf64_Ehdr h = get_elf_header(content);
        int begin = section_hdr.sh_offset;
        return content.substr(begin, section_hdr.sh_size);
    }

    /**
     * Get address of exact section header offset.
     * Called with num=0 returns adress of section header table.
     **/
    inline size_t get_sh_offset(Elf64_Ehdr hdr, size_t num) {
        return hdr.e_shoff + (num * hdr.e_shentsize);
    }




    std::string get_section_content(const std::string& name) {
        std::vector<section_descr> s_tbl = get_shdrs();
        Elf64_Shdr shdr = find_section(name, s_tbl);
        return get_section_content(shdr);
    }



    /**
     * Returns vector of all sections headers with it's names.
     **/
    std::vector<section_descr> get_shdrs() {
        Elf64_Ehdr h = get_elf_header(content);
        std::vector<Elf64_Shdr> s_hdrs;
        for (int i = 0; i < h.e_shnum; i++) {
            Elf64_Shdr header;
            std::memcpy(&header, &content[get_sh_offset(h, i)], h.e_shentsize);
            s_hdrs.push_back(header);
        }
        std::vector<section_descr> res;
        std::string shstr_content = get_section_content(s_hdrs[h.e_shstrndx]);

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
     * Returns last number of byte that belongs to given section.
     **/ 
    inline size_t section_end_offset(Elf64_Shdr hdr) {
        return hdr.sh_offset + hdr.sh_size;
    }


    size_t find_section_idx(const std::string& name, const std::vector<section_descr>& sections) {
        for (size_t i  = 0; i < sections.size(); i++) {
            if (sections[i].second == name)
                return i;
        }
        throw ("find_section: Cannot find section " + name + "\n");
    }

    Elf64_Shdr find_section(const std::string& name, const std::vector<section_descr>& sections) {
        return sections[find_section_idx(name, sections)].first;
    }
    

    /**
     * Adds offset to all sections after `sec_name` and increase `sec_name`'s size
     * by `num` value. 
     **/
    std::vector<section_descr>& add_offset(const std::string& sec_name, size_t num) {
        
        std::vector<section_descr> init_sec_tbl = get_shdrs();
        Elf64_Shdr sec_hdr = find_section(sec_name, init_sec_tbl);

        size_t my_offset = sec_hdr.sh_offset;

        for (auto & sec_descr : init_sec_tbl) {
            if (sec_descr.second == sec_name) {
                // edited one, change it's size
                sec_descr.first.sh_size += num;
            } 
            else if (sec_descr.first.sh_offset > my_offset) {
                // section is placed behind edited one
                // increase it's offset by numner of bytes inserted
                sec_descr.first.sh_offset += num;
            }
        }

        return init_sec_tbl;
    }

    /**
     * TODO na razie obsługuje ten sam size.
     */
    void replace_sec_hdr_tbl(std::vector<section_descr> new_tbl) {
        Elf64_Ehdr e_hdr = get_elf_header(content);
        
        assert(new_tbl.size() == e_hdr.e_shnum);

        size_t begin = e_hdr.e_shoff;
        size_t ent_size = e_hdr.e_shentsize;
        size_t num = ent_size * e_hdr.e_shnum;

        for (size_t i = 0; i < new_tbl.size(); i++) {
            size_t addr = e_hdr.e_shoff + i * ent_size;
            memcpy(&content[addr], &new_tbl[i].first, ent_size);
        }
    }

    /**
     * Inserts `what` string to end of `sec_name` section.
     * Returns std::string that represents changed binary.
     */
    void insert_to_section(const std::string& what, const std::string& sec_name) {
        
        std::vector<section_descr> init_sec_tbl = get_shdrs();
        

#define ELF_ASSERTS 1
#ifdef ELF_ASSERTS
        size_t lo_sec_addr = SIZE_MAX;
        for (auto s_hdr : init_sec_tbl) {
            lo_sec_addr = std::min(lo_sec_addr, s_hdr.first.sh_offset);
        }
        assert(get_elf_header(content).e_shoff < min_sec_addr);
#endif
        
        size_t sec_hdr_idx = find_section_idx(sec_name, init_sec_tbl);
        Elf64_Shdr sec_hdr = find_section(sec_name, init_sec_tbl);

        // TODO - logical error, `content` should be const (initial one) (or maybe no?)
        content.insert(section_end_offset(sec_hdr) + 1, what);


        auto new_sect_tbl = add_offset(sec_name, what.size());
        replace_sec_hdr_tbl(new_sect_tbl);
    }

    void dump(std::string out_file_path) {
        std::ofstream out;
        out.open(out_file_path);
        out << content;
        out.close();
    }

};