#ifndef SEGMENTTABLE_H
#define SEGMENTTABLE_H

#include <vector>
#include "segment.hpp"
using namespace std;

class SegmentTable{
private:
  ulong current_address;
public:
  vector<Segment*> segments;

  //SETTERS
  inline void increment_current_address_by(ulong u) {this->current_address += u;}
  inline void add_segment(Segment* s){this->segments.push_back(s);}

  //GETTERS
  inline ulong get_current_address() const {return this->current_address;}
  inline size_t get_size() const {return this->segments.size();}
  
  SegmentTable();
  ~SegmentTable();

};

SegmentTable::SegmentTable(){
  this->segments = vector<Segment*>();
  this->current_address = 0;
}

SegmentTable::~SegmentTable(){
  for(int i = 0; i < this->segments.size() ; ++i){
    delete this->segments[i];
  }
}

#endif