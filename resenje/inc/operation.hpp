#ifndef OPERATION_H
#define OPERATION_H

#include <string>
#include <bitset>
#include  "relocationTable.hpp"
using namespace std;

class Operation{
public:
  enum operand_types {
    LITERAL, SYMBOL, LITERAL_MEM, SYMBOL_MEM, REGISTER, REGISTER_MEM, REG_LITERAL_MEM, REG_SYMBOL_MEM, IMMEDIATE, IMMEDIATE_MEM ,UNDEFINED, REG_REG
  };
private:
  uint operation_code;
  string name;

  string gpr1;
  string gpr2;
  string operand;
  string csr;
  string shift_operation;
  operand_types operand_type;
  
public:
  Operation(uint c, string n);

  std::bitset<32> machine_code;
  bool is_special_ld_st;


  inline uint get_code() const {return this->operation_code;}
  inline string get_gpr1() const {return this->gpr1;}
  inline string get_gpr2() const {return this->gpr2;}
  inline string get_operand() const {return this->operand;}
  inline string get_shift_operation() const {return this->shift_operation;}
  inline string get_csr() const {return this->csr;}
  inline operand_types get_operand_type()const{return this->operand_type;}

  inline void set_name(string n){this->name = n;}
  inline void set_gpr1(string g){this->gpr1 = g;}
  inline void set_gpr2(string g){this->gpr2 = g;}
  inline void set_operand(string g){this->operand = g;}
  inline void set_shift_operation(string g){this->shift_operation = g;}
  inline void set_csr(string c){this->csr = c;}
  inline void set_operand_type(operand_types t){this->operand_type = t;}

};

Operation::Operation(uint c, string n){
  this->operation_code = c;
  this->name = n;
  this->gpr1 = "";
  this->gpr2 = "";
  this->csr = "";
  this->operand = "";
  this->operand_type = UNDEFINED;
  this->shift_operation = "";
  this->is_special_ld_st = false;
  this->machine_code = std::bitset<32>();
}


#endif