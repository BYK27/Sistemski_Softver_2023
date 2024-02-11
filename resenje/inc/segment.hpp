#ifndef SEGMENT_H
#define SEGMENT_H

#include "relocationTable.hpp"
#include "operation.hpp"

class Segment{
private:

  uint size;
  ulong start_address;
  bool defined;

  bool is_skip;
  bool is_operation;
  bool is_symbol;
  bool is_literal;

  Symbol* symbol;
  Operation* operation;
  Section* section;
  Relocation* relocation;
  
public:
  Segment();
  ~Segment();

  std::vector<std::bitset<8>> machine_code;

  //SETTERS
  inline void set_size(uint s){this->size = s;}
  inline void set_start_address(ulong s){this->start_address = s;}
  inline void set_symbol(Symbol* s){this->symbol = s;}
  inline void set_operation(Operation* o){this->operation = o;}
  inline void set_section(Section* s){this->section = s;}
  inline void set_relocation(Relocation* r){this->relocation = r;}
  inline void set_defined(){this->defined = true;}
  inline void set_contains_operation(){this->is_operation = true;}
  inline void set_contains_literal(){this->is_literal = true;}
  inline void set_contains_symbol(){this->is_symbol = true;}
  inline void set_skip(){this->is_skip = true;}
  void set_machine_code_by_stringvalue(string s);
 
  //GETTERS
  inline uint get_size()const{return this->size;}  //for modularity, project defines a segment as 4B
  inline ulong get_start_address() const {return this->start_address;}
  inline Symbol* get_symbol()const{return this->symbol;};
  inline Operation* get_operation()const{return this->operation;}
  inline Relocation* get_relocation()const{return this->relocation;}
  inline Section* get_section() const {return this->section;}
  inline bool isSkip()const{return this->is_skip;}
  inline bool isOperation()const{return this->is_operation;}
  inline bool isSymbol()const{return this->is_symbol;}
  inline bool isLiteral()const{return this->is_literal;}
  

};

Segment::Segment(){
  this->machine_code = std::vector<std::bitset<8>>();

  this->is_literal = false;
  this->is_operation = false;
  this->is_symbol = false;
  this->is_skip = false;
  this->size = 4;
} 


Segment::~Segment(){

  this->size = 0;
  this->start_address = 0;
  this->defined = false;
}

void Segment::set_machine_code_by_stringvalue(string s){
  this->machine_code.clear();
  //push machine code in segment, literal value of the literal 
  for (size_t i = 0; i < s.size(); i += 8) {
    std::string binaryChunk = s.substr(i, 8);
    this->machine_code.push_back(std::bitset<8>(binaryChunk));
  }
  std::reverse(this->machine_code.begin(), this->machine_code.end());
}

#endif