#include <elf.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>

using namespace std;

#define PH_ENT_SIZE 56 // we nned it in compile time for proper struct initialization

struct ph {
    char bytes[PH_ENT_SIZE];
} ph;


void integrity_check(Elf64_Ehdr h) {
    if(h.e_ident[1] != 'E' ||
       h.e_ident[2] != 'L' ||
       h.e_ident[3] != 'F') {
        throw "Given file is not in ELF format!";
    }
    if (h.e_phentsize != PH_ENT_SIZE) {
        throw "Program header entry size doesn't match predefined one!";
    }
}


/**
 * Reads ELF header from given file content.
 * Does integrity check, in case of error throws exception.
 **/
Elf64_Ehdr properElf(const std::string& content) {
    Elf64_Ehdr header;
    
    std::memcpy(&header, content.c_str(), sizeof(Elf64_Ehdr));
    // cout << hex << setfill('0') << setw(2) << header.e_ident ;
    cout << "LOL entry: " << header.e_phentsize << "\n"; 
    cout << "LOL off: " << header.e_phoff << "\n"; 
    
    integrity_check(header);

    return header;
}



void dump(std::string content, std::string out_file_path) {
    std::ofstream out;
    out.open(out_file_path);
    out << content;
    out.close();
}


int main() {
    ifstream exec_bin{"exec_syscall"};

    std::string exec_content((std::istreambuf_iterator<char>(exec_bin)),
                              std::istreambuf_iterator<char>());
    cout << "\nsize:" << exec_content.size();
    // cout << "\nsize:" << exec_content.size();
    // Elf32_Ehdr header;
    properElf(exec_content);
    // dump(exec_content, "tescik");
}