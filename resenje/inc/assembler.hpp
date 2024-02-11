#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include "segmentTable.hpp"
using namespace std;

class Assembler{
public:
  SymbolTable* symbol_table;
  RelocationTable* relocation_table;
  SegmentTable* segment_table;
  vector<Operation*> all_operations;
  vector<Section*> all_sections;

private:
  ifstream* input_file;
  ofstream* output_file;
  uint current_line;

  Section* current_section;
  
  unordered_map<string, int> operations = {
    {"halt", 0}, {"int", 1}, {"iret", 2}, {"call", 3}, {"ret", 4},
    {"jmp", 5}, {"beq", 6}, {"bne", 7}, {"bgt", 8}, {"push", 9}, {"pop", 10},
    {"xchg", 11}, {"add", 12}, {"sub", 13}, {"mul", 14}, {"div", 15}, {"not", 16},
    {"and", 17}, {"or", 18}, {"xor", 19}, {"shl", 20}, {"shr", 21}, {"ld", 22},
    {"st", 23}, {"csrrd", 24}, {"csrwr", 25}
  };
  unordered_map<string, int> directives = {
    {".global", 0}, {".extern", 1}, {".section", 2},
    {".word", 3}, {".skip", 4}, {".ascii", 5},
    {".equ", 6}, {"end", 7}
  };
  unordered_map<string, int> registers = {
    {"r0", 0}, {"r1", 1}, {"r2", 2}, {"r3", 3}, {"r4", 4}, {"r5", 5},
    {"r6", 6}, {"r7", 7}, {"r8", 8}, {"r9", 9}, {"r10", 10}, {"r11", 11},
    {"r12", 12}, {"r13", 13}, {"sp", 14}, {"pc", 15}
  };
  unordered_map<string, int> status_registers = {
    {"status",0}, {"handler", 1}, {"cause", 2}
  };

  //removes starting white spaces from string
  string clean_line(string l);
  //returns the first word of the line
  string get_word_line(string l);
  //removes all blanco spaces from string
  string remove_blanco_spaces(string l);

  //returns true if section already exists
  bool contains_section(string name);

  //adds a segment of 4bytes
  Segment* add_segment_for_operation(Operation* new_operation);

  //parser the registers
  void parseRegisters(const std::string& registers, std::string& sourceRegister, std::string& destinationRegister);
  void parseRegisters_and_shift(const std::string& registers, std::string& sourceRegister, std::string& destinationRegister, std::string& operation, std::string& operand);


  //DIRECTIVES
  void directive_global(string s);
  void directive_extern(string s);
  void directive_section(string s);
  void directive_word(string s);
  void directive_skip(string s);
  void directive_ascii(string s);
  void directive_equ(string s);
  void directive_end(string s);

  //MACHINE CODES
  void operation_halt(Operation* p);
  void operation_int(Operation* p);
  void operation_iret(Operation* p);
  void operation_call(Operation* p);
  void operation_ret(Operation* p);
  void operation_jmp(Operation* p);
  void operation_beq(Operation* p);
  void operation_bne(Operation* p);
  void operation_bgt(Operation* p);
  void operation_push(Operation* p);
  void operation_pop(Operation* p);
  void operation_xchg(Operation* p);
  void operation_add(Operation* p);
  void operation_add_special(Operation* p);
  void operation_sub(Operation* p);
  void operation_mul(Operation* p);
  void operation_div(Operation* p);
  void operation_not(Operation* p);
  void operation_and(Operation* p);
  void operation_or(Operation* p);
  void operation_xor(Operation* p);
  void operation_shl(Operation* p);
  void operation_shr(Operation* p);
  void operation_ld(Operation* p);
  void operation_st(Operation* p);
  void operation_csrrd(Operation* p);
  void operation_csrwr(Operation* p);
  
public:
  Assembler(ifstream* input, ofstream* output);
  ~Assembler();
  int first_pass();
  int second_pass();
  void print_section_table();
  void print_symbol_table();
  void print_segments();
  void print_segments_by_section();
  void print_relocation_table();
  void resolve_local_relocations();
 
  static uint next_section_id;
  static uint next_symbol_id;

  //CONVERTERS
  bool is_hex(string s);
  bool is_binary(string s);
  bool is_number(string& s);
  uint hex_to_int(string s);
  uint binary_to_int(string s);
  uint string_to_int(string s);
  string number_to_binary32_string(uint i);

  //GETTERS
  inline vector<Section*> get_all_sections() {return this->all_sections;}
  Section* get_section_by_name(string n);

  //SETTERS
  void add_symbol_to_symbol_table(Symbol* s){this->symbol_table->add_symbol(s);}

  void check_for_exceptions();
  
};
#endif