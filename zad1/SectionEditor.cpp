#include <iostream>
#include <string>
#include <elf.h>
#include <vector>
#include <cassert>
#include <algorithm>

#include <csignal>
#include <stdlib.h>

#include "Utils.hpp"
#include "SectionEditor.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

#include <stddef.h>


typedef SectionEditor SE;

/** Print a demangled stack backtrace of the caller function to FILE* out. */
static inline void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63)
{
    fprintf(out, "stack trace:\n");

    // storage array for stack trace address data
    void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if (addrlen == 0) {
	fprintf(out, "  <empty, possibly corrupt>\n");
	return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
	char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

	// find parentheses and +address offset surrounding the mangled name:
	// ./module(function+0x15c) [0x8048a6d]
	for (char *p = symbollist[i]; *p; ++p)
	{
	    if (*p == '(')
		begin_name = p;
	    else if (*p == '+')
		begin_offset = p;
	    else if (*p == ')' && begin_offset) {
		end_offset = p;
		break;
	    }
	}

	if (begin_name && begin_offset && end_offset
	    && begin_name < begin_offset)
	{
	    *begin_name++ = '\0';
	    *begin_offset++ = '\0';
	    *end_offset = '\0';

	    // mangled name is now in [begin_name, begin_offset) and caller
	    // offset in [begin_offset, end_offset). now apply
	    // __cxa_demangle():

	    int status;
	    char* ret = abi::__cxa_demangle(begin_name,
					    funcname, &funcnamesize, &status);
	    if (status == 0) {
		funcname = ret; // use possibly realloc()-ed string
		fprintf(out, "  %s : %s+%s\n",
			symbollist[i], funcname, begin_offset);
	    }
	    else {
		// demangling failed. Output function name as a C function with
		// no arguments.
		fprintf(out, "  %s : %s()+%s\n",
			symbollist[i], begin_name, begin_offset);
	    }
	}
	else
	{
	    // couldn't parse the line? print the whole line.
	    fprintf(out, "  %s\n", symbollist[i]);
	}
    }

    free(funcname);
    free(symbollist);
}



std::string SE::get_section_content(const std::string& content, Elf64_Shdr section_hdr) {
    Elf64_Ehdr h = get_elf_header(content);
    int begin = section_hdr.sh_offset;
    // char _res[section_hdr.sh_size];
    // memcpy(_res, content.data(), section_hdr.sh_size);
    // std::string res(_res, section_hdr.sh_size); // = content.substr(begin, section_hdr.sh_size);
    std::string res = content.substr(begin, section_hdr.sh_size);
    assert(res.size() == section_hdr.sh_size);
    return res;
}

// SectionEditor::SectionEditor(const std::string elf) : content(elf) {};


/**
 * Get address of exact section header offset.
 * Called with num=0 returns adress of section header table.
 **/
inline size_t SE::get_sh_offset(Elf64_Ehdr hdr, size_t num) {
    return hdr.e_shoff + (num * hdr.e_shentsize);
}

std::string SE::get_section_content(const std::string& content, const std::string& name) {
    std::vector<section_descr> s_tbl = get_shdrs(content);
    Elf64_Shdr shdr = find_section(name, s_tbl);
    return get_section_content(content, shdr);
}


/**
 * Returns vector of all sections headers with it's names.
 **/
std::vector<section_descr> SE::get_shdrs(const std::string& content) {
    Elf64_Ehdr h = get_elf_header(content);
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
 * Returns last number of byte that belongs to given section.
 **/ 
inline size_t SE::section_end_offset(Elf64_Shdr hdr) {
    return hdr.sh_offset + hdr.sh_size;
}


size_t SE::find_section_idx(const std::string& name, const std::vector<section_descr>& sections) {
    for (size_t i  = 0; i < sections.size(); i++) {
        if (sections[i].second == name)
            return i;
    }
    std::cerr << "find_section: Cannot find section " + name + "\n"; 
    throw ("find_section: Cannot find section " + name + "\n");
}


size_t SE::get_section_idx(const std::string& content, const std::string& name) {
    auto shdrs = SE::get_shdrs(content);
    return SE::find_section_idx(name, shdrs);
}


Elf64_Shdr SE::find_section(const std::string& name, const std::vector<section_descr>& sections) {
    return sections[find_section_idx(name, sections)].first;
}

/**
 * Used for finding ET_EXEC's mapped to virtual memory sections, thus
 * assert with nonzero return.
 */
size_t SE::get_section_vaddr(const std::string& content, const std::string& name) {
    std::cerr << "<<" << name << "<<\n";
    auto shdrs = SE::get_shdrs(content);
    auto shdr = SE::find_section(name, shdrs);
    assert(shdr.sh_addr != 0);
    return shdr.sh_addr;
}


/**
 *  *Adds offset to all sections after `sec_name` and increase `sec_name`'s size
 * by `num` value.
 *  *Updates indexes in .shstrtab (it occurs as a last section).
 *  *Updates elf header's .shstrtab offset.
 **/
void SectionEditor::add_offset(std::string& content, const std::string& sec_name, size_t num) {
    
    std::vector<section_descr> sec_tbl = get_shdrs(content);
    Elf64_Shdr sec_hdr = find_section(sec_name, sec_tbl);

    size_t my_offset = sec_hdr.sh_offset;

    for (auto & sec_descr : sec_tbl) {
        if (sec_descr.second == sec_name) {
            sec_descr.first.sh_size += num;
        }
        else if (sec_descr.first.sh_offset > my_offset) {
            // section is placed behind edited one
            // increase it's offset by number of bytes inserted
            sec_descr.first.sh_offset += num;
        }
    }

    replace_sec_hdr_tbl(content, sec_tbl);

    auto phdr_tbl = get_phs(content);
    // auto map = sec2seg_map(content);
    // we added bytes to section `sec_name`, that is
    for (Elf64_Phdr& p : phdr_tbl) {
        if (p.p_offset >= sec_hdr.sh_offset) {
            p.p_offset += num;
            p.p_vaddr += num;
            p.p_paddr += num;
        }
    }

    replace_pdhr_tbl(content, phdr_tbl);

    // update elf header .shstrtab offset

    Elf64_Ehdr ehdr = get_elf_header(content);
    
    if (ehdr.e_phoff > my_offset)
        ehdr.e_phoff += num;
        
    if (ehdr.e_shoff > my_offset)
        ehdr.e_shoff += num;
  
    content.replace(0, ehdr.e_ehsize, (const char*) &ehdr, ehdr.e_ehsize);
    
}


// ASSUMPTION: section header table is at the very end of file
void SectionEditor::append_sections(std::string& content, 
                                    std::vector<section_descr>& new_sections, 
                                    const std::vector<std::string>& new_sections_contents,
                                    const std::vector<size_t>& names_positions) {
    Elf64_Ehdr e_hdr = get_elf_header(content);

    assert(new_sections_contents.size() == new_sections.size());
    assert(content.size() == e_hdr.e_shoff + e_hdr.e_shnum * e_hdr.e_shentsize);

    auto actual_headers = SE::get_shdrs(content);
    
    content.erase(e_hdr.e_shoff, e_hdr.e_shnum * e_hdr.e_shentsize);

    assert(content.size() == e_hdr.e_shoff);
    size_t pos0 = content.size();
    // size_t name_idx = e_hdr.e_shnum; // first index of string array, will be useful later, for new sections
    
    for (size_t i = 0; i < new_sections.size(); i++) {
        content.append(new_sections_contents[i]);
    }


    e_hdr.e_shnum += new_sections.size();
    e_hdr.e_shoff = content.size();

    for (int i = 0; i < actual_headers.size(); i++) {
        content.append((const char*) &actual_headers[i], e_hdr.e_shentsize);
    }

    size_t off = pos0;

    for (size_t i = 0; i < new_sections.size(); i++) {
        Elf64_Shdr& hdr = new_sections[i].first;
        hdr.sh_addr = SE::MOVE_BASE + off + i * 0x200000;
        hdr.sh_offset = off;
        hdr.sh_name = names_positions[i];

        off += new_sections_contents[i].size();
    }

    for (int i = 0; i < new_sections.size(); i++) {
        content.append((const char*) &new_sections[i], e_hdr.e_shentsize);
    }

    // update elf header
    content.replace(0, sizeof(Elf64_Ehdr), (const char*) &e_hdr, sizeof(Elf64_Ehdr));
}


void SectionEditor::dump(const std::string& content, const std::string& out_file_path) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
    // chmod(out_file_path.data(), 755);
    std::string s("chmod 755 ");
    s.append(out_file_path);
    system(s.data());
}

size_t SectionEditor::append(std::string& content, const std::string& what) {
    size_t init_size = content.size();
    content.append(what);
    assert(init_size + what.size() == content.size());
    return init_size;
}

inline size_t get_section_offset(const std::string& content, const std::string sec_name) {
    return SE::find_section(sec_name, SE::get_shdrs(content)).sh_offset;
}


/**
 * ASSUMPTION:
 * ".shstrtab" is the last section, after it is only section header table.
 *  Returns vector of positions relevant to .shstrtab offset.
 */
std::vector<size_t> SectionEditor::add_moved_section_names(std::string& content, std::vector<section_descr>& sections_to_move) {
    std::vector<size_t> res;

    size_t s0 = content.size();
    size_t shstrtab_off = get_section_offset(content, ".shstrtab");
    Elf64_Ehdr ehdr = get_elf_header(content);
    
    size_t cut_off = 0;
    for (auto& pair : SE::get_shdrs(content)) {
        if (pair.second == ".shstrtab") {
            cut_off = pair.first.sh_offset + pair.first.sh_size; 
        }
    }
    assert(cut_off > 0);
    
    std::string cutted_str = content.substr(cut_off, std::string::npos);
    assert(cutted_str.size() >= ehdr.e_shnum * ehdr.e_shentsize);
    content.erase(cut_off, std::string::npos);

    assert(content.size() + cutted_str.size() == s0);

    size_t sum_size = 0;
    for (auto& pair : sections_to_move) {
        std::string& name = pair.second;

        const std::string prefix = "MOVED";
        std::string new_name = prefix + name;
        
        res.push_back(content.size() - shstrtab_off);
        content.append(new_name.data(), new_name.size());

        content.append("\0", 1);
        sum_size += new_name.size() + 1;
    }

    ehdr.e_shoff += sum_size;
    content.append(cutted_str);
    assert(content.size() == s0 + sum_size);

    content.replace(0, sizeof(Elf64_Ehdr), (const char*) &ehdr, sizeof(Elf64_Ehdr));

    auto sec_hdrs = SE::get_shdrs(content);

    for (auto& pair : sec_hdrs) {
        if (pair.second == ".shstrtab") {
            pair.first.sh_size += sum_size;
        }
    }

    replace_sec_hdr_tbl(content, sec_hdrs);

    return res;
}


void SectionEditor::replace_sec_hdr_tbl(std::string& content, std::vector<section_descr>& new_tbl) {
    Elf64_Ehdr e_hdr = get_elf_header(content);
    
    assert(new_tbl.size() == e_hdr.e_shnum);
    size_t begin = e_hdr.e_shoff;
    size_t ent_size = e_hdr.e_shentsize;
    
    for (size_t i = 0; i < new_tbl.size(); i++) {
        size_t addr = begin + i * ent_size;
        size_t s1 = content.size();
        content.replace(addr, ent_size, (const char*) &new_tbl[i].first, ent_size);
        size_t s2 = content.size();
        assert(s1 == s2);
    }
}