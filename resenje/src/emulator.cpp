#include "../inc/emulator.hpp"
#include <sstream>

Emulator::Emulator(ifstream* i){
  this->debug = true;    //set to true to generate output file with debug info
  this->current_address = 0x40000000;
  this->input_file = i;
  if(debug)this->output_file = new std::ofstream("emulation.txt");
  this->emulator_segment_table = new SegmentTable();
  this->parse_input_hex();
  //this->print_segments();
  this->emulate();
  //this->print_register_status();
}

Emulator::~Emulator(){
  this->input_file->close();
  if(debug)this->output_file->close();
  delete this->input_file;
  if(debug)delete this->output_file;
  delete this->emulator_segment_table;
}

void Emulator::parse_input_hex(){
  
  ulong start_addr = 0;

  bool end = false;
  string line = "";
  while(getline(*this->input_file, line) && !end){
    line = clean_line(line);
    if(line.size() > 0){
      Segment* segment1 = new Segment();
      Segment* segment2 = new Segment();
      size_t position = line.find("\t");
      string adr = line.substr(0, position);

      start_addr = this->hex_to_int(adr);
      segment1->set_start_address(start_addr);
      segment2->set_start_address(start_addr + 0x4);

      line.erase(0, position + 1);

      //line contains hex values
      line = clean_line(line);

      for(int column = 0 ; column < 4 ; ++column){
        size_t position = line.find("\t");
        string data = line.substr(0, position);
        line.erase(0,position + 1);
        line = clean_line(line);

        std::stringstream ss;
        ss << std::hex << data;
        unsigned int intValue;
        ss >> intValue;

        segment1->machine_code.push_back(std::bitset<8>(intValue));
      }

      for(int column = 0 ; column < 4 ; ++column){
        size_t position = line.find("\t");
        string data = line.substr(0, position);
        line.erase(0,position + 1);
        line = clean_line(line);

        std::stringstream ss;
        ss << std::hex << data;
        unsigned int intValue;
        ss >> intValue;

        segment2->machine_code.push_back(std::bitset<8>(intValue));
      }
      this->emulator_segment_table->add_segment(segment1);
      this->emulator_segment_table->add_segment(segment2);
      
    }
  }
}

void Emulator::emulate(){
  bool start = false;
  int i = 0 ;
  for( ; i < this->emulator_segment_table->segments.size(); ++i){
    //emulation can start
    if(this->emulator_segment_table->segments.at(i)->get_start_address() == this->starting_address) {
      start = true;
      break;
    }
  }

  if(!start) throw new ExceptionAlert("No segment can currently execute.");

  this->registers[15] = this->starting_address; //program counter points to the next instruction
  this->current_address = this->starting_address;  //instruction currently executing

  //special instructions insert a new segment into memory as part of literal pool
  //these instructions are made of two parts, so the PC is incremented twice
  bool last_instruction_jump = false;
  i = 0;

  while(start){

    this->current_address = this->registers[0xF];
    this->registers[0xF] += 0x4; 
    Segment* current_segment = this->get_segment_by_address(this->current_address);

    last_instruction_jump = false;

    //I - 7654
    string I4_string = current_segment->machine_code.at(0).to_string().substr(0,4);
    bitset<4> I4_bits = bitset<4>(I4_string);
    ulong I4_number = I4_bits.to_ullong();

    //I - 3210
    string I0_string = current_segment->machine_code.at(0).to_string().substr(4);
    bitset<4> I0_bits = bitset<4>(I0_string);
    ulong I0_number = I0_bits.to_ullong();

    //II - 7654
    string II4_string = current_segment->machine_code.at(1).to_string().substr(0,4);
    bitset<4> II4_bits = bitset<4>(II4_string);
    ulong II4_number = II4_bits.to_ullong();

    //II - 3210
    string II0_string = current_segment->machine_code.at(1).to_string().substr(4);
    bitset<4> II0_bits = bitset<4>(II0_string);
    ulong II0_number = II0_bits.to_ullong();

    //III - 7654
    string III4_string = current_segment->machine_code.at(2).to_string().substr(0,4);
    bitset<4> III4_bits = bitset<4>(III4_string);
    ulong III4_number = III4_bits.to_ullong();

    //III - 3210
    string III0_string = current_segment->machine_code.at(2).to_string().substr(4);
    bitset<4> III0_bits = bitset<4>(III0_string);
    ulong III0_number = III0_bits.to_ullong();

    //IV - 7654
    string IV4_string = current_segment->machine_code.at(3).to_string().substr(0,4);
    bitset<4> IV4_bits = bitset<4>(IV4_string);
    ulong IV4_number = IV4_bits.to_ullong();

    //IV - 3210
    string IV0_string = current_segment->machine_code.at(3).to_string().substr(4);
    bitset<4> IV0_bits = bitset<4>(IV0_string);
    ulong IV0_number = IV0_bits.to_ullong();

    bitset<12> D_bits = bitset<12>(III0_string + IV4_string + IV0_string);

    bool D_is_negative = D_bits[11];

    ulong M = I0_number;
    ulong A = II4_number;
    ulong B = II0_number;
    ulong C = III4_number;    
    ulong D_signed = D_bits.to_ullong();
    int D = 0;

    if (D_is_negative) {
      // Apply sign extension by setting bits 12-63 to 1 (for a 64-bit signed value)
      for(int k = 0 ; k < 12 ; ++k){
        if(D_bits[k]) D_bits.reset(k);
        else D_bits.set(k);
      }
      D = D_bits.to_ullong() + 1;
      D *= -1;
    } else {
        D = static_cast<int>(D_signed);
    }


    if(debug){
      *this->output_file<<"\n" << std::to_string(i) << "\t"; 
      this->print_register_temp();
    }

    switch (I4_number)
    {
    case 0x0: //HALT
    {
      start = false;
      this->print_register_status();
      return;
      break;
    }
    case 0x1: //INT
    {
      if(debug)*this->output_file << "INT" << "\n";
      this->push_status();
      this->push_pc();
      this->status_registers[2] = 4;
      this->status_registers[0] &= (~0x4);
      this->registers[0xF] = this->status_registers[1];
      last_instruction_jump = true;
      break;
    }
    case 0x2: //CALL
    {
      if(debug)*this->output_file << "CALL ";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file << "pc <= gpr" << std::to_string(A)<< " + gpr"<< std::to_string(B) <<" + " << std::to_string(D) << "\n";
        this->push_pc();
        this->registers[0xF] = this->registers[A] + this->registers[B] + D;
        break;
      }
      case 0x1:{
        if(debug)*this->output_file << "pc <= mem[gpr" << std::to_string(A)<< " + gpr"<< std::to_string(B) <<" + " << std::to_string(D) << "]\n";
        this->push_pc_special();
        Segment* that_segment = this->get_segment_by_address(this->registers[A] + this->registers[B] + D);
        this->registers[0xF] = this->get_segment_value(that_segment);
        break;
      }
      
      default:
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      last_instruction_jump = true;
      break;
    }
    case 0x3: //JMP
    {
      if(debug)*this->output_file << "JMP ";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file << "pc = reg" << std::to_string(A) << " + " << std::to_string(D) << "\n";
        this->registers[15] = this->registers[A] + D;
        last_instruction_jump = true;
        break;
      }
      case 0x1:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "== reg"<<std::to_string(C) << "then pc = reg" << std::to_string(A) << " + " << std::to_string(D) ;
        if(this->registers[B] == this->registers[C]) {*this->output_file << "CLEAR\n";this->registers[15] <= this->registers[A] + D; last_instruction_jump = true;}
        break;
      }
      case 0x2:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "!= reg"<<std::to_string(C) << "then pc = reg" << std::to_string(A) << " + " << std::to_string(D) << "\n";
        if(this->registers[B] != this->registers[C]) {*this->output_file << "CLEAR\n";this->registers[15] <= this->registers[A] + D; last_instruction_jump = true;}
        break;
      }
      case 0x3:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "> reg"<<std::to_string(C) << "then pc = reg" << std::to_string(A) << " + " << std::to_string(D) << "\n";
        if(static_cast<uint32_t>(this->registers[B]) > static_cast<uint32_t>(this->registers[C])) {*this->output_file << "CLEAR\n";this->registers[15] <= this->registers[A] + D;last_instruction_jump = true;}
        break;
      }
      case 0x8:{
        last_instruction_jump = true;
        Segment* that_segment = this->get_segment_by_address(this->registers[A] + D);

        uint32_t segment_value = this->get_segment_value(that_segment);
        if(debug)*this->output_file << "pc = mem[" << std::to_string(A) << " + " << std::to_string(D) << "]\n";
        this->registers[15] = segment_value;
        break;
      }
      case 0x9:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "== reg"<<std::to_string(C) << "then pc = mem[reg" << std::to_string(A) << " + " << std::to_string(D) << "]";
        if(this->registers[B] == this->registers[C]) {
          
          Segment* that_segment = this->get_segment_by_address(this->registers[A] + D);
          uint32_t segment_value = this->get_segment_value(that_segment);
          this->registers[15] = segment_value;
          last_instruction_jump = true; //pc is updated by instruction
        }
        break;
      }
      case 0xA:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "!= reg"<<std::to_string(C) << "then pc = mem[reg" << std::to_string(A) << " + " << std::to_string(D) << "]";
        if(this->registers[B] != this->registers[C]) {
          
          Segment* that_segment = this->get_segment_by_address(this->registers[A] + D);
          uint32_t segment_value = this->get_segment_value(that_segment);
          this->registers[15] = segment_value;
          last_instruction_jump = true; //pc is updated by instruction
        }
        break;
      }
      case 0xB:{
        if(debug)*this->output_file << "if reg"<<std::to_string(B)<< "> reg"<<std::to_string(C) << "then pc = mem[reg" << std::to_string(A) << " + " << std::to_string(D) << "]";
        if(this->registers[B] != this->registers[C]) {
          
          Segment* that_segment = this->get_segment_by_address(this->registers[A] + D);
          uint32_t segment_value = this->get_segment_value(that_segment);
          this->registers[15] = segment_value;
          last_instruction_jump = true; //pc is updated by instruction
        }
        break;
      }
      default:
        //throw new ExceptionAlert("No such JMP operation exists.");
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      break;
    }
    case 0x4: //gpr1 <=> gpr2
    {
      if(debug)*this->output_file << "SWITCH reg" <<std::to_string(B)<< "and"<<std::to_string(C)<<"\n";
      ulong temp = this->registers[B]; 
      this->registers[B] = this->registers[C];
      this->registers[C] = temp;
      break;
    }
    case 0x5: //addition, subtraction, multiplication, division
    {
      if(debug)*this->output_file << "ARITHMETIC";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"+r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] + this->registers[C];
        break;
      }
      case 0x1:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"-r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] - this->registers[C];
        break;
      }
      case 0x2:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"*r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] * this->registers[C];
        break;
      }
      case 0x3:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"/r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] / this->registers[C];
        break;
      }
      case 0x4:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"/r" <<std::to_string(C)<<"\n";
        
        if(III0_number == 0)
        {
          this->registers[A] = this->registers[B] + (this->registers[C] << D);
        }
        else
        {
          this->registers[A] = this->registers[B] + (this->registers[C] >> D);
        }
        break;
      }
      default:
        //throw new ExceptionAlert("No such Arithmetic operation exists.");
        std::string exact_error = "Unknown operation code: " + I4_string + " "+ I0_string +" - " + II4_string + " "+ II0_string +" - "+ III4_string +" " + III0_string +" - "+ IV0_string + " " + IV4_string;

        throw ExceptionAlert(exact_error);
        break;
      }
      break;
    }
    case 0x6: //not, and, or, nor 
    {
      if(debug)*this->output_file << "LOGIC ";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = ~"  <<"r" <<std::to_string(B) << "\n";
        this->registers[A] = ~this->registers[B];
        break;
      }
      case 0x1:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"&r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] & this->registers[C];
        break;
      }
      case 0x2:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"|r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] | this->registers[C];
        break;
      }
      case 0x3:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"^r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] ^ this->registers[C];
        break;
      }
      default:
        //throw new ExceptionAlert("No such Logical operation exists.");
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      break;
    }
    case 0x7: //shift left, shift right
    {
      if(debug)*this->output_file << "SHIFT ";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<"<<r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] << this->registers[C];
        break;
      }
      case 0x1:{
        if(debug)*this->output_file <<"r" <<std::to_string(A) << " = "  <<"r" <<std::to_string(B)<<">>r" <<std::to_string(C)<<"\n";
        this->registers[A] = this->registers[B] >> this->registers[C];
        break;
      }
      default:
        //throw new ExceptionAlert("No such Shift operation exists.");
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      break;
    }
    case 0x8: //st, push
    {
      if(debug)*this->output_file << "ST or PUSH\t";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file << "ST mem[gpr"<<std::to_string(A)<<" + gpr" <<std::to_string(B)<<" + " << std::to_string(D) << "] r" << std::to_string(C)<< "\n";
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[A] + D);
        
        string segment_value = this->number_to_binary32_string(this->registers[C]);
        that_segment->set_machine_code_by_stringvalue(segment_value);

        break;
      }
      case 0x1:{
        if(debug)*this->output_file << "PUSH r" <<std::to_string(C) << "\n"; 
        this->registers[A] += D;
        Segment* that_segment = this->get_segment_by_address(this->registers[A]);
        string symbol_value = this->number_to_binary32_string(this->registers[C]);
        that_segment->set_machine_code_by_stringvalue(symbol_value);
        break;
      }
      case 0x2:{
        
        if(debug)*this->output_file << "ST mem[mem[gpr"<<std::to_string(A)<<" + gpr" <<std::to_string(B)<<" + " << std::to_string(D) << "]] r" << std::to_string(C);
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[A] + D);
        if(debug)*this->output_file << "with value " << std::to_string(this->get_segment_value(that_segment));
        that_segment = this->get_segment_by_address(this->get_segment_value(that_segment));
        string segment_value = this->number_to_binary32_string(this->registers[C]);
        //WATCH
        that_segment->set_machine_code_by_stringvalue(segment_value);
        if(debug)*this->output_file << " so the final mem value is " << std::to_string(this->get_segment_value(that_segment)) << "\n";

        break;
      }
      case 0x3:
      {
        if(debug)*this->output_file << "ST mem[gpr"<<std::to_string(A)<<" + gpr" <<std::to_string(B)<<" + " << std::to_string(D) << "] r" << std::to_string(C)<< "\n";
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[A] + D);
        
        string segment_value = this->number_to_binary32_string(this->registers[C]);
        that_segment->set_machine_code_by_stringvalue(segment_value);

        break;
      }
      
      default:
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      break;
    }
    case 0x9: //ld, pop
    {
      if(debug)*this->output_file << "LD ";
      switch (M)
      {
      case 0x0:{
        if(debug)*this->output_file << "csr"<<std::to_string(B)<< " r" << std::to_string(A)<< "\n";
        this->registers[A] = this->status_registers[B];
        break;
      }
      case 0x1:{
        if(debug)*this->output_file << "gpr "<<std::to_string(B)<<" + " << std::to_string(D) << " r" << std::to_string(A)<< "\n";
        this->registers[A] = this->registers[B] + D;
        break;
      }
      case 0x2:{
        if(debug)*this->output_file << "[gpr "<<std::to_string(B)<<" + " << std::to_string(D) << "] r" << std::to_string(A)<< "\n";
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[C] + D);
        
        uint32_t segment_value = this->get_segment_value(that_segment);
        this->registers[A] = segment_value;
       
        break;
      }
      case 0x3:{
        if(debug)*this->output_file << "actually POP "<<std::to_string(A) << "\n";
        Segment* that_segment = this->get_segment_by_address(this->registers[B]);
        uint32_t segment_value = this->get_segment_value(that_segment);

        this->registers[A] = segment_value;
        this->registers[B] += D;

        //if next instruction is POP status, then we have IRET and it must be done atomically
        Segment* helper_segment = this->get_segment_by_address(this->current_address + 0x4);
        uint32_t helper_value = this->get_segment_value(helper_segment);
        string result_address = "";
        for(int h = 0 ; h < 4 ; ++h){
          result_address+=helper_segment->machine_code.at(h).to_string();
        }
        bitset<32> binary_value(result_address);
        helper_value =  binary_value.to_ullong();
        if(helper_value == 0x970E0004){
          if(debug)*this->output_file << "+ POP STATUS = IRET\n";
          Segment* next_segment = this->get_segment_by_address(this->registers[0xE]);
          uint32_t new_segment_value = this->get_segment_value(next_segment);
          this->status_registers[0] = new_segment_value;
          this->registers[0xE] += 4;
        }

        break;
      }
      case 0x4:{
        if(debug)*this->output_file << "r"<<std::to_string(B)<< " csr" << std::to_string(A)<< "\n";
        this->status_registers[A] = this->registers[B];
        break;
      }
      case 0x5:{
        this->status_registers[A] = this->status_registers[B] | D;
        break;
      }
      case 0x6:{
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[C] + D);

        uint32_t segment_value = this->get_segment_value(that_segment);

        this->status_registers[A] = segment_value;
        break;
      }
      case 0x7:{
        Segment* that_segment = this->get_segment_by_address(this->registers[B]);
        this->registers[B] += D;

        uint32_t segment_value = this->get_segment_value(that_segment);

        this->status_registers[A] = segment_value;
        break;
      }

      case 0x8:
      {
        Segment* that_segment = this->get_segment_by_address(this->registers[B] + this->registers[C] + D);
        
        uint32_t segment_value = this->get_segment_value(that_segment);
        this->registers[A] = segment_value;
        break;
      }
      
      default:
        throw ExceptionAlert("Unknown operation code.");
        break;
      }
      break;
    }
      
    
    default:
      throw ExceptionAlert("Unknown operation code.");
      break;
    }

    ++i;

  }
}

void Emulator::print_segments(){
  printf("SEGMENT TABLE\n");
  *this->output_file << "\nSEGMENT TABLE\n";
  for(int i = 0 ; i < this->emulator_segment_table->segments.size(); ++i){
    
    *this->output_file << std::to_string(i) << "\t";

    std::stringstream stream;
    stream << std::hex << this->emulator_segment_table->segments.at(i)->get_start_address();
    std::string hexString = stream.str();
    *this->output_file << hexString << "\t";


    for(int j = 0 ; j < this->emulator_segment_table->segments.at(i)->machine_code.size() ; ++j){
      *this->output_file << this->emulator_segment_table->segments.at(i)->machine_code.at(j).to_string() << " ";

    }
    *this->output_file << "\n";
  }
}

void Emulator::print_register_status(){
  int hexWidth = 8;
  std::cout << "Emulated processor executed halt instruction.\n" << "Emulated processor state\n";
  for(int i = 0 ; i < 16; ++i){
    std::stringstream stream;
    stream << std::hex << std::setw(hexWidth) << std::setfill('0')  << this->registers[i];
    string hexString = stream.str();
    std::cout<<"r"<<std::to_string(i) <<"=0x" << hexString << "\t";
    if (i >= 3 && (i - 3) % 4 == 0) std::cout << "\n";
  }
}

void Emulator::print_register_temp(){
  int hexWidth = 2;
  *this->output_file <<"\nREGISTERS\n";
  std::stringstream stream;
  stream << std::hex << std::setw(hexWidth) << std::setfill('0')  << this->current_address;
  string hexString = stream.str();
  *this->output_file <<"Current address: 0x"<< hexString<<"\n";

  for(int i = 0 ; i < 16; ++i){
    std::stringstream stream;
    stream << std::hex << std::setw(hexWidth) << std::setfill('0')  << this->registers[i];
    string hexString = stream.str();
    *this->output_file<<"r"<<std::to_string(i) <<"=0x" << hexString << "\n";
  }
  for(int i = 0 ; i < 3; ++i){
    std::stringstream stream;
    stream << std::hex << std::setw(hexWidth) << std::setfill('0')  << this->status_registers[i];
    string hexString = stream.str();
    *this->output_file<<"c"<<std::to_string(i) <<"=0x" << hexString << "\n";
  }
  *this->output_file << "\n";
}

Segment* Emulator::get_segment_by_address(ulong a){
  for(int i = 0 ; i < this->emulator_segment_table->segments.size(); ++i)
  {
    if(this->emulator_segment_table->segments.at(i)->get_start_address() == a) return this->emulator_segment_table->segments.at(i);
  }
  //make a NEW SEGMENT;

  std::stringstream stream;
  stream << std::hex << a;
  string hexString = stream.str();

  if(debug) std::cout << "No segment, allocate a new one at address 0x" << hexString << "\n";

  Segment* new_segment = new Segment();
  new_segment->set_start_address(a);
  for(int j = 0; j < 4; ++j){
    bitset<8> bitChunk("00000000");
    new_segment->machine_code.push_back(bitChunk);
  }
  this->emulator_segment_table->add_segment(new_segment);
  return new_segment;
}

//removes starting blanco spaces
string Emulator::clean_line(string l){
  string new_line = "";
  uint i = 0;
  //ignore all white spaces
  while(l[i] == ' ' || l[i] == '\t') ++i;
  //stop until EOT is reached
  while(l[i] != '#' && l[i] != '\0'){
    new_line += l[i++];
  }
  return new_line;
}

uint Emulator::hex_to_int(string s){

  unsigned long decimalValue = std::stoul(s, nullptr, 0); // Convert hexadecimal string to decimal value
  uint32_t uintValue = static_cast<uint32_t>(decimalValue); // Convert the decimal value to a uint32_t

  return uintValue;
}

uint Emulator::string_to_int(string s){
  try {
        return stoi(s);
    } catch (const std::invalid_argument& e) {
        throw new ExceptionAlert("Invalid argument for string to int method.");
        return -1;
    } catch (const std::out_of_range& e) {
        throw new ExceptionAlert("Argument out of range for string to int method.");
        return -1;
    }
}

string Emulator::stringify_segment(Segment* s){
  string result_address = "";
  for(int j = 0 ; j < 4 ; ++j){
    string that_chunk = s->machine_code.at(j).to_string();
    result_address+=s->machine_code.at(j).to_string();
  }
  return result_address;
}

uint32_t Emulator::get_segment_value(Segment* s){
  string result_address = "";
  for(int j = 3 ; j >= 0 ; --j){
    result_address+=s->machine_code.at(j).to_string();
  }
  bitset<32> binary_value(result_address);
  return binary_value.to_ullong();
}

string Emulator::number_to_binary32_string(uint i){
  std::bitset<32> binary_representation(i);
  string binary_string = binary_representation.to_string();
  return binary_string;
}

void Emulator::push_pc(){
  this->registers[0xE] -= 0x4;
  Segment* that_segment = this->get_segment_by_address(this->registers[0xE]);
  string symbol_value = this->number_to_binary32_string(this->registers[0xF]);

  that_segment->machine_code.clear();
  //push machine code in segment, literal value of the literal 
  for (size_t i = 0; i < symbol_value.size(); i += 8) {
    std::string binaryChunk = symbol_value.substr(i, 8);
    that_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
  }

  std::reverse(that_segment->machine_code.begin(), that_segment->machine_code.end());
}

void Emulator::push_pc_special(){
  this->registers[0xE] -= 0x4;
  Segment* that_segment = this->get_segment_by_address(this->registers[0xE]);
  string symbol_value = this->number_to_binary32_string(this->registers[0xF] + 0x4);

  that_segment->machine_code.clear();
  //push machine code in segment, literal value of the literal 
  for (size_t i = 0; i < symbol_value.size(); i += 8) {
    std::string binaryChunk = symbol_value.substr(i, 8);
    that_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
  }

  std::reverse(that_segment->machine_code.begin(), that_segment->machine_code.end());
}

void Emulator::push_status(){
  this->registers[0xE] -= 0x4;
  Segment* that_segment = this->get_segment_by_address(this->registers[0xE]);
  string symbol_value = this->number_to_binary32_string(this->status_registers[0x0]);

  that_segment->machine_code.clear();
  //push machine code in segment, literal value of the literal 
  for (size_t i = 0; i < symbol_value.size(); i += 8) {
    std::string binaryChunk = symbol_value.substr(i, 8);
    that_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
  }

  std::reverse(that_segment->machine_code.begin(), that_segment->machine_code.end());
}