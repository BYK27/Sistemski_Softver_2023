#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H
#include <vector>
#include "symbol.hpp"
using namespace std;

class SymbolTable{
  public:
  vector<Symbol*> all_symbols;

  inline void add_symbol(Symbol* s){all_symbols.push_back(s);}
  inline Symbol* get_symbol(int i){return all_symbols.at(i);}
  Symbol* get_symbol_by_name(string s);
  void remove_symbol_by_name(string s);
  bool contains_symbol(string name);
  size_t get_size()const{return this->all_symbols.size();}

  SymbolTable();
  ~SymbolTable();
  
};


//returns true if the symbol table contains that symbol
bool SymbolTable::contains_symbol(string name){
  for(int i = 0 ; i < all_symbols.size() ; ++i){
    if(all_symbols.at(i)->get_name() == name) return true;
  }
  return false;
}

Symbol* SymbolTable::get_symbol_by_name(string s){
  for(int i = 0 ; i < this->all_symbols.size() ; ++i){
    if(all_symbols.at(i)->get_name() == s) return all_symbols.at(i);
  }
  return nullptr;
}

void SymbolTable::remove_symbol_by_name(string s){
  for(int i = 0 ; i < this->all_symbols.size() ; ++i){
    if(all_symbols.at(i)->get_name() == s){
      this->all_symbols.at(i) = nullptr;
      for (int j = i; i < this->all_symbols.size() - 1; ++j)
      {
        this->all_symbols[j] = this->all_symbols[j + 1];
      }
      return;
    }
  }
}

SymbolTable::SymbolTable(){
  this->all_symbols = vector<Symbol*>();
}

SymbolTable::~SymbolTable(){
  for(int i = 0 ; i < this->all_symbols.size(); ++i){
    delete this->all_symbols.at(i);
  }
}

#endif