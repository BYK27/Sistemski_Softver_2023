#ifndef EMULATOR_H
#define EMULATOR_H
#include "linker.hpp"
#include <iomanip>

class Emulator{
private:
  ulong current_address;
  const ulong starting_address = 0x40000000;
  ifstream* input_file;
  ofstream* output_file;
  uint32_t registers[16]={0}; //pc is reg15, sp is reg14, all registers are initialized to 0;
  uint32_t status_registers[3]={0}; //status, handler, cause
  bool debug; //used for printing instructions and registers in a file

  SegmentTable* emulator_segment_table;

  string clean_line(string l);
  uint hex_to_int(string s);
  uint string_to_int(string s);
  string stringify_segment(Segment * s);
  uint32_t get_segment_value(Segment* s);
  string number_to_binary32_string(uint i);
  void parse_input_hex();
  void emulate();

  void print_segments();
  void print_register_status();
  void print_register_temp();

  Segment* get_segment_by_address(ulong a);

  void push_pc();
  void push_pc_special();
  void push_status();

public:
  Emulator(ifstream* i);
  ~Emulator();
};

#endif