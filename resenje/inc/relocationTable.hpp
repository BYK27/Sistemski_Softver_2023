#ifndef RELOCATIONTABLE_H
#define RELOCATIONTABLE_H

#include <vector>
#include "relocation.hpp"
using namespace std;

class RelocationTable{
public:
  vector<Relocation*> all_relocations;
  inline void add_relocation(Relocation* r) {this->all_relocations.push_back(r);}

  RelocationTable();
  ~RelocationTable();
};

RelocationTable::RelocationTable(){
  this->all_relocations = vector<Relocation*>();
}

RelocationTable::~RelocationTable(){
  for(int i = 0 ; i < this->all_relocations.size(); ++i){
    delete this->all_relocations.at(i);
  }
}

#endif