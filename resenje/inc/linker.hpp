#ifndef LINKER_H
#define LINKER_H

#include <vector>
#include <bitset>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include "assembler.hpp"
using namespace std;

class Linker{
private:

  bool debug; //used for printing linkers section, symbol, relocation and segment table into a file 

  vector<Section*> all_sections;
  SymbolTable* global_symbol_table;
  RelocationTable* global_relocation_table;
  SegmentTable* global_segment_table;
  vector<ifstream*> input_files;
  ofstream* output_file;
  ofstream* output_file_debug;
  unordered_map<string, uint32_t> place_arguments;
  vector<Symbol*> unresolved_symbols;
  ulong current_address;

  bool contains_section(string n);
  bool contains_symbol(string n);
  Section* get_section_by_name(string n);
  Section* get_highest_section()const;
  ulong find_highest_address();
  bool check_for_section_overlaps();
  bool section_overlap(Section* a, Section* b);
  void move_overlapping_sections(Section* s);
  void resolve_symbols();
  Symbol* get_symbol_by_name(string n);
  Symbol* get_symbol_by_value(uint v);
  Segment* get_segment_by_start_address(ulong a);

  bool compareSegmentsByAddress(const Segment* a, const Segment* b) {return a->get_start_address() < b->get_start_address();}

  //parsing input files
  void add_symbol_from_input(string l);
  void add_section_from_input(string l, ifstream* file);
  void add_relocation_from_input(string l);

  void add_symbols_from_section(Section* s, string l, ulong addend, ifstream* file);
  void add_relocations_from_section(Section*, string l, ulong addend, ifstream* file);
  void add_segments_from_section(Section*, string l, ulong addend, ifstream* file);

  //string helper functions
  string clean_line(string l);
  uint string_to_int(string s);
  uint hex_to_int(string s);
  string number_to_binary32_string(uint i);

public:
  Linker(vector<ifstream*> i, ofstream* o, unordered_map<string, uint32_t> p);
  ~Linker();
  void merge_sections();
  void merge_symbols();
  void merge_relocations();

  void merge_section_symbols(Section* s, ulong addend, ifstream* file);
  void merge_section_relocations(Section* s, ulong addend, ifstream* file);
  void merge_section_segments(Section* s, ulong addend, ifstream* file);

  //printing functions
  void print_section_table();
  void print_symbol_table();
  void print_relocation_table();
  void print_segment_table();
  void print_hex();
  
  void change_section_id_unique();
  void change_symbol_id_unique();
  void resolve_relocation_symbols();
  void resolve_relocations();

};

#endif