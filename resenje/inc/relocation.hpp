#ifndef RELOCATION_H
#define RELOCATION_H
#include "symbolTable.hpp"

class Relocation{
public:
  enum relocations {ABSOLUTE, RELATIVE};

private:
  Symbol* symbol;   //which value needs to be built into the machine code
  Section* section; //where the symbol needs to be placed
  ulong offset; //from the start of the section where we need to put the symbol value
  ulong addend;
  ulong end;  //depricated, DELETE
  relocations type;
  string name;

public:
  Relocation();
  ~Relocation();

  //SETTERS
  inline void set_symbol(Symbol* s){this->symbol = s;}
  inline void set_section(Section* s){this->section = s;}
  inline void set_offset(ulong i){this->offset = i;}
  inline void set_end(ulong i){this->end = i;}
  inline void set_relocation(Relocation::relocations r){this->type = r;}
  inline void set_addend(ulong i){this->addend = i;}
  inline void set_name(string n){this->name = n;}

  //GETTERS
  inline Symbol* get_symbol()const{return this->symbol;}
  inline Section* get_section()const{return this->section;}
  inline ulong get_offset()const{return this->offset;}
  inline ulong get_addend()const{return this->addend;}
  inline string get_name()const{return this->name;}
};

Relocation::Relocation(){
  this->symbol = nullptr;
  this->section = nullptr;
  this->offset = 0;
  this->end = 0;
  this->addend = 0;
  this->type = Relocation::ABSOLUTE;
  this->name = "";
}

Relocation::~Relocation(){

}

#endif