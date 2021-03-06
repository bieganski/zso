#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>


#include "Utils.hpp"
#include "SectionEditor.hpp"

using namespace std;

using symbol_descr = std::pair<Elf64_Sym, std::string>;

typedef struct rela_descr {
    Elf64_Rela hdr;
    symbol_descr symbol;
    size_t vaddr;
} rela_descr;


typedef SectionEditor SE;

size_t BASE_REL;

const static u_int64_t EXEC_BASE = 0x400000;

std::string random_string( size_t length ) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    /* using nano-seconds instead of seconds */
    srand((time_t)ts.tv_nsec);

    auto randchar = []() -> char {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}

std::string PREFIX = random_string(5);


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

        if (rel_content.size() == 0) {
            throw rel_fname.data();
        }

        if (exec_content.size() == 0) {
            throw exec_fname.data();
        }

        return std::make_pair(exec_content, rel_content);
    } catch (const char * f) {
        std::cerr << "ERROR: Cannot open " << f << "!\n";
        exit(1);
    }
}

size_t compute_num_additional_pages(size_t num_new_load_sections, size_t phentsize) {
    size_t additional_bytes = num_new_load_sections * phentsize;
    size_t num_pages = additional_bytes / getpagesize() + 1;
    return num_pages;
}

Elf64_Word get_phflags(Elf64_Xword sh_flags) {
    Elf64_Word flags = 0;

    flags |= PF_R;

    if (sh_flags & SHF_WRITE) {
        flags |= PF_W;
    }

    if (sh_flags & SHF_EXECINSTR) {
        flags |= PF_X;
    }

    return flags;
}

size_t vaddr2off(const std::string& exec_content, size_t vaddr) {
    auto phdrs = get_phs(exec_content);

    for (const auto& ph : phdrs) {
        if (ph.p_type == PT_LOAD) {
            if (vaddr >= ph.p_vaddr && vaddr <= ph.p_vaddr + ph.p_memsz) {
                size_t off = vaddr - ph.p_vaddr;
                return ph.p_offset + off;
            }
        }
    }
    std::stringstream err;
    std::cerr << "Internal error: Adress " << hex << vaddr <<  " isn't mapped into memory";
    throw "Internal error: vaddr2off";
}

std::vector<symbol_descr> get_symbols(const std::string& content) {
    auto shdrs = SE::get_shdrs(content);
    Elf64_Shdr symtab = SE::find_section(".symtab", shdrs);
    std::string strtab_content = SE::get_section_content(content, ".strtab");
    std::vector<symbol_descr> res;

    assert(symtab.sh_size % sizeof(Elf64_Sym) == 0);
    for (size_t i = 0; i < symtab.sh_size; i+=sizeof(Elf64_Sym)) {
        Elf64_Sym sym;
        size_t addr = i + symtab.sh_offset;
        memcpy(&sym, &content.data()[addr], sizeof(Elf64_Sym));
        std::string s(&strtab_content.data()[sym.st_name]); // to first null char
        res.push_back(std::make_pair(sym, s));
    }
    return res;
}

symbol_descr find_corresponding_symbol(const std::string& exec_content, symbol_descr rel_sym) {
    auto exec_symbols = get_symbols(exec_content);
    if (rel_sym.second == "orig_start") {
        rel_sym.second = "_start";
    }
    for (auto pair : exec_symbols) {
        if (pair.second == rel_sym.second) {
            return pair;
        }
    }
    std::cerr << "Linking error: Malformed binary: No " + rel_sym.second + " symbol in ET_EXEC file!";
    exit(1);
}

size_t get_rela_vaddr(const std::string& content, const std::string& sec_name, Elf64_Rela r) {
    size_t vaddr = SE::get_section_vaddr(content, sec_name);
    return vaddr + r.r_offset;
}

std::vector<section_descr> get_rela_sections(const std::string& content) {
    auto pairs = SE::get_shdrs(content);
    std::vector<section_descr> res;
    for (auto& pair : pairs) {
        if (pair.first.sh_type == SHT_RELA)
            res.push_back(pair);
    }
    assert(res.size() > 0);
    return res;
}

std::vector<rela_descr> get_rela_entries(const std::string& exec_content, const std::string& rel_content) {
    std::vector<rela_descr> res;
    auto rela_sections = get_rela_sections(rel_content);
    auto rel_symbols = get_symbols(rel_content);
    for(auto& rela : rela_sections) {
        assert(rela.second.substr(0, 5) == ".rela");
        
        Elf64_Rela r;
        assert(rela.first.sh_size % sizeof(Elf64_Rela) == 0);
        for (size_t i = 0; i < rela.first.sh_size; i+=sizeof(Elf64_Rela)) {
            Elf64_Rela r;
            size_t addr = i + rela.first.sh_offset;
            memcpy(&r, &rel_content.data()[addr], sizeof(Elf64_Rela));

            size_t sym_idx = ELF64_R_SYM(r.r_info);
            symbol_descr rel_sym = rel_symbols[sym_idx];

            // now, we have symbol that maybe is in ET_EXEC, 
            // but first check whether it occurs in ET_REL.
            symbol_descr result_sym;

            std::string sec_name = rela.second.substr(5, std::string::npos);
            if (sec_name == ".eh_frame") {
                continue;
            }
            sec_name.insert(0, PREFIX);

            bool from_rel = false;
            for (auto pair : rel_symbols) {
                if (pair.second == rel_sym.second && rel_sym.first.st_shndx != SHN_UNDEF) {
                    // here we go, simply we must change section name to new one
                    result_sym = rel_sym;
                    std::string sym_sec_name = SE::get_shdrs(rel_content)[rel_sym.first.st_shndx].second;
                    sym_sec_name.insert(0, PREFIX);

                    size_t rel_offset = rel_sym.first.st_value; // in ET_REL st_value field keeps offset from `st_shndx` begin
                    result_sym.first.st_value = rel_offset + SE::get_section_vaddr(exec_content, sym_sec_name);
                    
                    from_rel = true;
                }
            }
            if (!from_rel) {
                // we have obtained UND symbol from ET_REL,
                // now we must obtain corresponding one from
                // ET_EXEC, thus find it by name
                result_sym = find_corresponding_symbol(exec_content, rel_sym);
            }

            rela_descr res_rela {
                .hdr = r,
                .symbol = result_sym,
                .vaddr = get_rela_vaddr(exec_content, sec_name, r),
            };
            res.push_back(res_rela);
        }
    }
    return res;
}

void set_base(const std::string exec_content) {
    auto phdrs = get_phs(exec_content);
    size_t max = 0;
    for (auto& p : phdrs) {
        if (p.p_type == PT_LOAD) {
            max = std::max(max, p.p_paddr - (p.p_paddr % 0x200000));
        }
    }
    
    BASE_REL = max + 0x200000;
}


/**
 * This should be simple, but it's not because of
 * need to printf unformatted number bytes - it needs
 * stream's `write` method.
 * Does a lot of copies, may be slow for large binaries.
 */
void execute_relocation(std::string& content, size_t rel_val, size_t offset, bool rel64 = false) {
    std::stringstream ss;

    size_t num = rel64 ? 8 : 4;
    size_t s0 = content.size();

    ss << content.substr(0, offset);
    ss.write((const char *) &rel_val, num);
    ss << content.substr(offset + num, std::string::npos);

    content.clear();
    content = ss.str();

    assert (s0 == content.size());
}

void resolve_relocations(std::string& exec_content, const std::string& rel_content) {
    auto symbols = get_symbols(exec_content);
    auto relas = get_rela_entries(exec_content, rel_content);

    size_t s0 = exec_content.size();

    for (rela_descr r : relas) {
        size_t rela_off = vaddr2off(exec_content, r.vaddr);
        auto shdrs = SE::get_shdrs(exec_content);
        size_t rel_val = r.symbol.first.st_value;
        rel_val += r.hdr.r_addend;
        if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_PC32) {
            rel_val -= r.vaddr; // relative
            execute_relocation(exec_content, rel_val, rela_off);

        } else if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_PC64) {
            rel_val -= r.vaddr; // relative
            execute_relocation(exec_content, rel_val, rela_off, true);

        } else if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_32) {
            execute_relocation(exec_content, rel_val, rela_off);

        } else if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_64) {
            execute_relocation(exec_content, rel_val, rela_off, true);

        } else if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_PLT32) {
            rel_val -= r.vaddr; // relative
            execute_relocation(exec_content, rel_val, rela_off);
        } else if (ELF64_R_TYPE(r.hdr.r_info) == R_X86_64_32S) {
            execute_relocation(exec_content, rel_val, rela_off);
        } else {
            std::cerr << "[INFO] omitting relocation for symbol " << r.symbol.second << " (not supported type)\n";
        }
    }
    assert(s0 == exec_content.size());
}


void overwrite_start(std::string& exec_content, const std::string& rel_content) {
    auto rel_symbols = get_symbols(rel_content);
    Elf64_Ehdr ehdr = get_elf_header(exec_content);

    std::string new_start_section;
    size_t rel_off;

    bool is_start = false;
    for (const auto& pair : rel_symbols) {
        if (pair.second == "_start") {
            new_start_section = PREFIX + SE::get_shdrs(rel_content)[pair.first.st_shndx].second;
            size_t moved_text_vaddr = SE::get_section_vaddr(exec_content, new_start_section);
            rel_off = pair.first.st_value;
            ehdr.e_entry = moved_text_vaddr + rel_off;
            exec_content.replace(0, sizeof(Elf64_Ehdr), (const char *) &ehdr, sizeof(Elf64_Ehdr));
            is_start = true;
        }
    }
    if (!is_start) {
        std::cerr <<  "[INFO] Lack of _start symbol in ET_REL! e_entry not overridden\n";
        return;
    }
    
    auto exec_symbols = get_symbols(exec_content);
    for (size_t i = 0; i < exec_symbols.size(); i++) {
        if (exec_symbols[i].second == "_start") {
            Elf64_Sym s = exec_symbols[i].first; // we need to change it's offset and shndx
            size_t idx = SE::get_section_idx(exec_content, new_start_section);
            s.st_shndx = idx;
            s.st_value = rel_off + SE::get_section_vaddr(exec_content, new_start_section);
            Elf64_Shdr symtab = SE::find_section(".symtab", SE::get_shdrs(exec_content));
        
            exec_content.replace(symtab.sh_offset + i * sizeof(Elf64_Sym), sizeof(Elf64_Sym), (const char *) &s, sizeof(Elf64_Sym));
        }
    }
}


int main(int argc, char** argv) {

    if (argc != 4) {
        std::cerr << "Usage: ./postlinker <ET_EXEC file> <ET_REL file> <target ET_EXEC file>\n";
        exit(1);
    }

    auto input_pair = read_input_elfs(argv[1], argv[2]);
    
    std::string exec_content = input_pair.first;
    std::string rel_content = input_pair.second;

    try {
        integrity_check(get_elf_header(exec_content));
    } catch (const char * s) {
        std::cerr << argv[1] << " error: " << s << "\n";
        exit(1);
    }

    try {
        integrity_check(get_elf_header(rel_content));
    } catch (const char * s) {
        std::cerr << argv[2] << " error: " << s << "\n";
        exit(1);
    }

    set_base(exec_content);

    std::vector<section_descr> sections_to_move;
    for (auto pair : SE::get_shdrs(rel_content)) {
        Elf64_Shdr hdr = pair.first;
        if (hdr.sh_flags & SHF_ALLOC) {
            sections_to_move.push_back(pair);
        }
    }

    std::vector<std::string> moved_sections_contents;

    for (auto& pair : sections_to_move) {
        Elf64_Shdr& hdr = pair.first;
        string& name = pair.second;

        moved_sections_contents.push_back(SE::get_section_content(rel_content, name));
    }

    SE::append_sections(exec_content, sections_to_move, moved_sections_contents);

    SE::add_moved_section_names(exec_content, sections_to_move, PREFIX);

    // now, we will be inserting place for new program headers

    Elf64_Ehdr exec_hdr = get_elf_header(exec_content);

    size_t num_new_phdrs = 1 + sections_to_move.size(); // '1' for additional PT_LOAD with new headers
    size_t num_pages_begin = compute_num_additional_pages(num_new_phdrs, exec_hdr.e_phentsize);
    size_t whole_size = getpagesize() * num_pages_begin;
    std::string begin_buf;

    exec_hdr.e_shoff += whole_size;
    exec_hdr.e_phoff = exec_hdr.e_ehsize; // just behind elf header
    exec_hdr.e_phnum += num_new_phdrs;

    begin_buf.append((const char*) &exec_hdr, sizeof(Elf64_Ehdr));
    auto phdrs = get_phs(exec_content);

    // first PT_LOAD segment maps elf header and program headers.
    // create it manually.
    Elf64_Phdr first_load {
        .p_type = PT_LOAD,
        .p_flags = PF_R | PF_X,
        .p_offset = 0,
        .p_vaddr = EXEC_BASE - whole_size,
        .p_paddr = EXEC_BASE - whole_size,
        .p_filesz = exec_hdr.e_ehsize + (num_new_phdrs + phdrs.size()) * exec_hdr.e_phentsize,
        .p_memsz = exec_hdr.e_ehsize + (num_new_phdrs + phdrs.size()) * exec_hdr.e_phentsize,
        .p_align = (Elf64_Xword) 1,
    };

    int phdr_pos = -1, _i = 0;
    for (auto& ph : phdrs) {
        if (ph.p_type == PT_PHDR) {
            phdr_pos = _i;
            size_t addr = EXEC_BASE - whole_size + exec_hdr.e_phoff;
            ph.p_paddr = addr;
            ph.p_vaddr = addr;
        } else {
            ph.p_offset += whole_size;
        }
        _i++;
    }

    // generate new program headers and add offsets to existing program headers table
    std::vector<Elf64_Phdr> new_phdrs;
    size_t j = 0;
    for (const auto& pair: sections_to_move) {
        Elf64_Shdr shdr = pair.first;
        size_t ADDR = j * 0x200000 + BASE_REL + shdr.sh_offset;
        Elf64_Phdr phdr {
            .p_type = PT_LOAD,
            .p_flags = get_phflags(shdr.sh_flags),
            .p_offset = whole_size + shdr.sh_offset,
            .p_vaddr = ADDR,
            .p_paddr = ADDR,
            .p_filesz = shdr.sh_type & SHT_NOBITS ? 0 : shdr.sh_size,
            .p_memsz = shdr.sh_size,
            .p_align = 0x200000, // max page size
        };
        j++;
        new_phdrs.push_back(phdr);
    }

    phdrs.insert(phdrs.end(), new_phdrs.begin(), new_phdrs.end());

    if (phdr_pos != -1) {
        for(size_t i = 0; i <= phdr_pos; i++) {
            size_t addr = exec_hdr.e_ehsize + i * exec_hdr.e_phentsize;
            begin_buf.append((const char*) &phdrs[i], sizeof(Elf64_Phdr));
        }
    }

    begin_buf.append((const char*) &first_load, sizeof(Elf64_Phdr));

    for (size_t i = phdr_pos + 1; i < phdrs.size(); i++) {
        size_t addr = exec_hdr.e_ehsize + (i + 1) * exec_hdr.e_phentsize;
        begin_buf.append((const char*) &phdrs[i], sizeof(Elf64_Phdr));
    }


    // Add offsets to section headers table
    auto shdrs = SE::get_shdrs(exec_content);
    for (auto &pair : shdrs) {
        Elf64_Shdr& shdr = pair.first;
        shdr.sh_offset += whole_size;
    }
    SE::replace_sec_hdr_tbl(exec_content, shdrs);

    begin_buf.resize(whole_size, '\0');
    exec_content.insert(0, begin_buf);

    resolve_relocations(exec_content, rel_content);

    overwrite_start(exec_content, rel_content);

    SE::dump(exec_content, argv[3]);
}
