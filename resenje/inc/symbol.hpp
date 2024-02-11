#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>
#include "section.hpp"

#define uint u_int32_t
#define ulong unsigned long

using namespace std;


class Symbol{
public:
  enum bindings {LOCAL, GLOBAL};

private:
  uint id;
  uint value;
  string name;
  Section* section;
  bool is_defined; 
  bool is_symbol;
  bool is_literal;
  bool is_section;
  bool is_extern;
  bindings binding;

public:
  Symbol();
  ~Symbol();

  //GETTERS
  inline uint get_id()const {return this->id;}
  inline int get_value() {return this->value;}
  inline string get_name(){return this->name;}
  inline Section* get_section(){return this->section;}
  inline bool isDefined(){return this->is_defined;}
  inline bool isSymbol()const{return this->is_symbol;}
  inline bool isLiteral()const{return this->is_literal;}
  inline bool isSection()const{return this->is_section;}
  inline bindings get_binding()const{return this->binding;}
  inline bool get_extern()const{return this->is_extern;}


  //SETTERS
  inline void set_id(uint i){this->id = i;}
  inline void set_value(uint v){this->value = v;}
  inline void set_name(string n){this->name = n;}
  inline void set_section(Section* s){this->section = s;}
  inline void set_isSection(){this->is_section = true;}
  inline void set_defined(){this->is_defined = true;}
  inline void set_isSymbol(){this->is_symbol = true;}
  inline void set_isLiteral(){this->is_literal = true;}
  inline void set_binding(Symbol::bindings b){this->binding = b;}
  inline void set_extern(){this->is_extern = true;}
};


//LOCAL binding is default
Symbol::Symbol(){
  this->id = 0;
  this->value = 0;
  this->name = "";
  this->section = nullptr;
  this->is_defined = false;
  this->is_symbol = false;
  this->is_literal = false;
  this->is_section = false;
  this->binding = Symbol::LOCAL;
  this->is_extern = false;
}

Symbol::~Symbol(){
  this->section = nullptr;
}

#endif