#ifndef SECTION_H
#define SECTION_H

#include <iostream>
#include <string.h>
#include <vector>
#include "exceptionAlert.hpp"
using namespace std;


class Section{
public:

private:
  uint id;
  string name;
  ulong start_address;
  ulong location_counter;
  ulong size;

public:
  Section();

  //setters
  inline void set_id(uint i){this->id = i;}
  inline void set_name(string n){this->name = n;}
  inline void set_start_address(ulong s){this->start_address = s;}
  inline void set_size(ulong s){this->size = s;}
  inline void increment_location_counter_by(ulong i){this->location_counter += i;}
  inline void increment_size_by(ulong i){this->size += i;}

  //getters
  inline uint get_id()const{return this->id;}
  inline string get_name()const{return this->name;}
  inline ulong get_start_address()const{return this->start_address;}
  inline ulong get_location_counter()const{return this->location_counter;}
  inline ulong get_size()const{return this->size;}
};

Section::Section(){
  this->id = 0;
  this->name = "";
  this->start_address = 0;
  this->location_counter = 0;
  this->size = 0;
}


#endif