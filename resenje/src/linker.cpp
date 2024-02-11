#include "../inc/linker.hpp"
#include <iostream>
#include <iomanip>


Linker::Linker(vector<ifstream*> i, ofstream* o, unordered_map<string, uint32_t> p){
  this->debug = true;
  if(debug) this->output_file_debug = new ofstream("aplication_debug.txt");
  this->input_files = i;
  this->output_file = o;
  this->place_arguments = p;
  this->global_symbol_table = new SymbolTable();
  this->global_relocation_table = new RelocationTable();
  this->global_segment_table = new SegmentTable();
  this->all_sections = vector<Section*>();
  this->unresolved_symbols = vector<Symbol*>();
  this->current_address = 0;

  this->merge_sections();
  if(debug){
    this->print_section_table();
    this->print_symbol_table();
    this->print_relocation_table();
    this->print_segment_table();
  }
  this->resolve_relocations();
  if(debug){
    *this->output_file_debug << "\nAFTER SYMBOL RESOLVING\n";
    this->print_segment_table();
  }
  
}

Linker::~Linker(){
  delete global_relocation_table;
  delete global_segment_table;
  delete global_symbol_table;
  this->output_file->close();
  for(int i = 0 ; i < this->input_files.size(); ++i){
    this->input_files.at(i)->close();
  }
  if(debug) this->output_file_debug->close();
}

void Linker::merge_sections(){
  
  //go through all .o input files
  for(int file = 0 ; file < this->input_files.size() ; ++file){

    bool end = false;
    bool start = false;
    string line = "";

    while(getline(*this->input_files.at(file), line) && !end){
      line = clean_line(line);

      if(line.size() > 0){

        if(line == "END_SECTION_TABLE" || line == "SEGMENT_TABLE" || line == "RELOCATION_TABLE") {end = true; break;}
        
        if(start){
          std::streampos original_position = this->input_files.at(file)->tellg(); //record last position before recursion
          if(line.find("ID") == std::string::npos) this->add_section_from_input(line, this->input_files.at(file));
          this->input_files.at(file)->seekg(original_position);  //restore previous state
        }

        if(line == "SECTION_TABLE" ) start = true;
      }
    }
  }
  this->change_section_id_unique();
  this->change_symbol_id_unique();
  this->resolve_relocation_symbols();
}

void Linker::merge_symbols(){
  //go through all .o input files
  for(int file = 0 ; file < this->input_files.size() ; ++file){

    bool end = false;
    bool start = false;
    string line = "";

    while(getline(*this->input_files.at(file), line) && !end){
      line = clean_line(line);

      if(line.size() > 0){

        if(line == "END_SYMBOL_TABLE") {end = true; break;}
        
        if(start){
          if(line.find("ID") == std::string::npos) this->add_symbol_from_input(line);
        }

        if(line == "SYMBOL_TABLE") start = true;
      }
    }
  }
  this->resolve_symbols();
}

void Linker::merge_section_symbols(Section* s, ulong addend, ifstream* file){
    bool end = false;
    bool start = false;
    string line = "";


    while(getline(*file, line) && !end){
      line = clean_line(line);

      if(line.size() > 0){

        if(line == "END_SYMBOL_TABLE") {end = true; break;}
        
        if(start){
          if(line.find("ID") == std::string::npos) this->add_symbols_from_section(s, line, addend, file);
        }

        if(line == "SYMBOL_TABLE") start = true;
      }
    }
}

void Linker::merge_section_relocations(Section* s, ulong addend, ifstream* file){
  bool end = false;
  bool start = false;
  string line = "";

  while(getline(*file, line) && !end){
    line = clean_line(line);

    if(line.size() > 0){

      if(line == "END_RELOCATION_TABLE") {end = true; break;}
      
      if(start){
        if(line.find("NAME") == std::string::npos) this->add_relocations_from_section(s, line, addend, file);
      }

      if(line == "RELOCATION_TABLE") start = true;
    }
  }
}

void Linker::merge_section_segments(Section* s, ulong addend, ifstream* file){
  bool end = false;
  bool start = false;
  string line = "";

  while(getline(*file, line) && !end){
    line = clean_line(line);

    if(line.size() > 0){

      if(line == "END_SEGMENT_TABLE") {end = true; break;}
      
      if(line.find("SECTION") != std::string::npos) start = false;

      if(start){
        this->add_segments_from_section(s, line, addend, file);
      }

      if(line.find("SECTION") != std::string::npos) {
        start = false;
        //found SECTION tag, telling us in which section the machine code needs to be placed
        size_t position_sec = line.find(" ");
        line.erase(0, position_sec);
        line = clean_line(line);


        //segment belongs to that section, we can proccess it
        if(line == s->get_name()){
          start = true;
        }
      }
    }
  }
}

void Linker::merge_relocations(){
  //go through all .o input files
  for(int file = 0 ; file < this->input_files.size() ; ++file){

    bool end = false;
    bool start = false;
    string line = "";

    while(getline(*this->input_files.at(file), line) && !end){
      line = clean_line(line);

      if(line.size() > 0){

        if(line == "END_RELOCATION_TABLE") {end = true; break;}
        
        if(start){
          if(line.find("NAME") == std::string::npos) this->add_relocation_from_input(line);
        }

        if(line == "RELOCATION_TABLE") start = true;
      }
    }
  }
}

void Linker::add_symbols_from_section(Section* s, string l, ulong addend, ifstream* file){

  Symbol* new_symbol = new Symbol();

  for(int column = 0 ; column < 5 ; ++column){
    size_t position = l.find("\t\t");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);

    switch (column)
    {
    case 0: //id
    {
      new_symbol->set_id(this->string_to_int(data));
      break;
    }
    case 1: //name
    {
      new_symbol->set_name(data);
      if(this->contains_section(data)){
        delete new_symbol;
        return;
      }
      break;
    }
    case 2: //value
    {
      new_symbol->set_value(this->hex_to_int(data));
      break;
    }
    case 3: //binding
    {
      new_symbol->set_binding(data == "LOCAL" ? Symbol::LOCAL : Symbol::GLOBAL);
      break;
    }
    case 4: //section
    {
      //if it belongs to that section
      if(data == s->get_name()){
        new_symbol->set_section(this->get_section_by_name(data));
        new_symbol->set_value(new_symbol->get_value() + addend);
        this->global_symbol_table->add_symbol(new_symbol);
      }
      //symbol belongs to a different section and has to be disregarded
      else{
        delete new_symbol;
      }
      break;
    }
    
    default:
      break;
    }

  }
}

void Linker::add_relocations_from_section(Section* s, string l, ulong addend, ifstream* file){

  Relocation* new_relocation = new Relocation();

  for(int column = 0 ; column < 4 ; ++column){
    size_t position = l.find("\t\t");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);

    switch (column)
    {
    case 0: //name
    {
      //new_relocation->set_symbol(this->get_symbol_by_name(data));
      new_relocation->set_name(data);
      break;
    }
    case 1: //section
    {
      new_relocation->set_section(this->get_section_by_name(data));
      break;
    }
    case 2: //offset
    {
      if(new_relocation->get_section()->get_name() == s->get_name())
      {
        new_relocation->set_offset(this->hex_to_int(data) + addend);
      }
      else{
        delete new_relocation;
        return;
      }
      break;
    }
    case 3: //addend
    {
      new_relocation->set_addend(this->hex_to_int(data));
      break;
    }
    
    default:
      break;
    }
  }
  this->global_relocation_table->add_relocation(new_relocation);
}

void Linker::add_segments_from_section(Section* s, string l, ulong addend, ifstream* file){

  Segment* new_segment = new Segment();
  
  size_t position_t = l.find("\t");
  string address = l.substr(0, position_t); //get binary chunk's address in hex
  l.erase(0, position_t);
  l = clean_line(l);

  new_segment->set_start_address(this->hex_to_int(address) + addend);

  for(int column = 0 ; column < 4 ; ++column){
    size_t position = l.find(" ");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);

    new_segment->machine_code.push_back(std::bitset<8>(data));
  }

  this->global_segment_table->add_segment(new_segment);
}

void Linker::add_symbol_from_input(string l){

  Symbol* new_symbol = new Symbol();

  for(int column = 0 ; column < 5 ; ++column){
    size_t position = l.find("\t\t");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);

    switch (column)
    {
    case 0: //id
    {
      new_symbol->set_id(this->string_to_int(data));
      break;
    }
    case 1: //name
    {
      new_symbol->set_name(data);
      break;
    }
    case 2: //value
    {
      new_symbol->set_value(this->hex_to_int(data));
      break;
    }
    case 3: //binding
    {
      new_symbol->set_binding(data == "LOCAL" ? Symbol::LOCAL : Symbol::GLOBAL);
      break;
    }
    case 4: //section
    {
      //new_symbol->set_section(this->get_section_by_name(data));
      if(data != "UND"){
        new_symbol->set_section(this->get_section_by_name(data));
        this->global_symbol_table->add_symbol(new_symbol);
        
      }
      else{
        this->unresolved_symbols.push_back(new_symbol);
      }
      break;
    }
    
    default:
      break;
    }

    
  }
}

void Linker::add_section_from_input(string l, ifstream* file){

  Section* new_section = new Section();

  //make new section
  for(int column = 0 ; column < 4 ; ++column){
    size_t position = l.find("\t\t");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);
    switch (column)
    {
    case 0: //id
    {
      new_section->set_id(this->string_to_int(data));
      break;
    }
    case 1: //name
    {
      new_section->set_name(data);
      break;
    }
    case 2: //start address
    {
      new_section->set_start_address(this->hex_to_int(data));
      break;
    }
    case 3: //size (location_counter)
    {
      new_section->set_size(this->hex_to_int(data));
      break;
    }
    
    default:
      break;
    }
  }

  if(new_section->get_size() == 0) {delete new_section; return;}

  uint addend = 0;

  //section already in linker's section table
  if(this->contains_section(new_section->get_name())){
    Section* that_section = this->get_section_by_name(new_section->get_name());
    addend = that_section->get_size() + that_section->get_start_address();
    that_section->increment_size_by(new_section->get_size());
    this->move_overlapping_sections(that_section);
    delete new_section;    
    this->merge_section_symbols(that_section, addend, file);
    this->merge_section_relocations(that_section, addend, file);
    this->merge_section_segments(that_section, addend, file);
  }
  else {
    //check if the starting address is specified for the section
    std::unordered_map<std::string, uint32_t>::iterator it = this->place_arguments.find(new_section->get_name());

    //name specified in place arguments
    if (it != this->place_arguments.end()) 
    {
      //std::cout << "Key found: " << it->first << ", Value: " << it->second << std::endl;
      ulong specified_address = it->second;
      new_section->set_start_address(specified_address);
    }
    //place in default location, after the end of a section placed on the highest address
    else 
    {
      Section* highest_section = this->get_highest_section();
      ulong new_start_address = highest_section == nullptr ? 0 : highest_section->get_start_address() + highest_section->get_size();
      new_section->set_start_address(new_start_address);
    }
    addend = new_section->get_start_address();
    this->all_sections.push_back(new_section);

    Symbol* new_symbol = new Symbol();
    new_symbol->set_value(new_section->get_start_address());
    new_symbol->set_name(new_section->get_name());
    new_symbol->set_section(new_section);
    new_symbol->set_defined();
    new_symbol->set_isSection();
    new_symbol->set_binding(Symbol::LOCAL);
    this->global_symbol_table->add_symbol(new_symbol);

    this->merge_section_symbols(new_section, addend, file);
    this->merge_section_relocations(new_section, addend, file);
    this->merge_section_segments(new_section, addend, file);
  }
}

void Linker::add_relocation_from_input(string l){

  Relocation* new_relocation = new Relocation();

  for(int column = 0 ; column < 3 ; ++column){
    size_t position = l.find("\t\t");
    string data = l.substr(0, position);
    l.erase(0,position + 1);
    l = clean_line(l);

    switch (column)
    {
    case 0: //name
    {
      new_relocation->set_symbol(this->get_symbol_by_name(data));
      break;
    }
    case 1: //section
    {
      new_relocation->set_section(this->get_section_by_name(data));
      break;
    }
    case 2: //offset
    {
      new_relocation->set_offset(this->hex_to_int(data));
      break;
    }
    
    default:
      break;
    }
  }
  this->global_relocation_table->add_relocation(new_relocation);
}

//removes starting blanco spaces
string Linker::clean_line(string l){
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

uint Linker::string_to_int(string s){
  try {
        return stoi(s);
    } catch (const std::invalid_argument& e) {
        throw ExceptionAlert("Invalid argument for string to int method.");
        return -1;
    } catch (const std::out_of_range& e) {
        throw ExceptionAlert("Argument out of range for string to int method.");
        return -1;
    }
}

uint Linker::hex_to_int(string s){

  unsigned long decimalValue = std::stoul(s, nullptr, 0); // Convert hexadecimal string to decimal value
  uint32_t uintValue = static_cast<uint32_t>(decimalValue); // Convert the decimal value to a uint32_t

  return uintValue;
}

bool Linker::contains_section(string n)
{
  for(int i = 0 ; i < this->all_sections.size() ; ++i){
    if(this->all_sections.at(i)->get_name() == n) return true;
  }
  return false;
}

Section* Linker::get_section_by_name(string n){
  for(int i = 0 ; i < this->all_sections.size() ; ++i){
    if(this->all_sections.at(i)->get_name() == n) return this->all_sections.at(i);
  }
  throw ExceptionAlert("Section not found during linking.");
  return nullptr;
}

ulong Linker::find_highest_address(){
  ulong highest = 0;
  Section* that_section = nullptr;
  for(int j = 0 ; j < this->all_sections.size(); ++j)
  {
    if(this->all_sections.at(j)->get_start_address() > highest){
      highest = this->all_sections.at(j)->get_start_address();
      that_section = this->all_sections.at(j);
    }
  }
  return highest;
}

bool Linker::section_overlap(Section* a, Section* b){
  ulong a_end = a->get_start_address() + a->get_size();
  return a_end > b->get_start_address();
}

bool Linker::check_for_section_overlaps(){
  for(int i = 0 ; i < this->all_sections.size() - 1; ++i){
    if(this->section_overlap(this->all_sections.at(i) , this->all_sections.at(i+1))){
      throw ExceptionAlert("Section overlap in linker.");
      return false;
    }
  }
  return true;
}

Section* Linker::get_highest_section() const{
  ulong highest = 0;
  Section* that_section = nullptr;
  for(int j = 0 ; j < this->all_sections.size(); ++j)
  {
    if(this->all_sections.at(j)->get_start_address() >= highest){
      highest = this->all_sections.at(j)->get_start_address();
      that_section = this->all_sections.at(j);
    }
  }
  return that_section;
}

void Linker::move_overlapping_sections(Section* s){
  for(int i = 0; i < this->all_sections.size(); ++i){
    //sections overlap and must be rearanged
    if(this->all_sections.at(i) != s && this->section_overlap(this->all_sections.at(i), s)){
      //section has a sepcified placing argument and cannot be rearranged
      std::unordered_map<std::string, uint32_t>::iterator it = this->place_arguments.find(this->all_sections.at(i)->get_name());
      //name specified in place arguments
      if (it != this->place_arguments.end()){
        throw ExceptionAlert("Section overlap cannot be resolved due to placing arguments specified in the command line.");
      }
      else{
        this->all_sections.at(i)->set_start_address(s->get_start_address() + s->get_size());
      }
    }

  }
}

void Linker::print_section_table(){
  *this->output_file_debug << "\nSECTION_TABLE\n";
  *this->output_file_debug << "\nID\t\tNAME\t\tADDRESS\t\tSIZE\n";
  for(int i = 0 ; i < this->all_sections.size(); ++i){
    std::stringstream stream;
    std::stringstream stream2;
    ulong addrs = this->all_sections.at(i)->get_start_address();
    ulong loc = this->all_sections.at(i)->get_size();
    stream << std::hex << addrs;
    stream2 << std::hex << loc;
    std::string hexStringA = stream.str();
    std::string hexStringL = stream2.str();
    *this->output_file_debug << std::to_string(this->all_sections.at(i)->get_id())<<"\t\t"<< this->all_sections.at(i)->get_name() <<"\t\t0x" << hexStringA << "\t\t0x" << hexStringL << "\n";
  }
  *this->output_file_debug << "\nEND_SECTION_TABLE\n";
}

void Linker::print_symbol_table(){
  *this->output_file_debug << "\nSYMBOL_TABLE\n";
  *this->output_file_debug << "\nID\t\tNAME\t\tVALUE\t\tBINDING\t\tSECTION\n";
  string bind = "";
  string sec = "";
  for(int i = 0 ; i < this->global_symbol_table->get_size() ; ++i){
    bind = this->global_symbol_table->get_symbol(i)->get_binding() == Symbol::GLOBAL ? "GLOBAL" : "LOCAL";
    sec = this->global_symbol_table->get_symbol(i)->get_section()->get_name();
    std::stringstream stream;
    ulong val = this->global_symbol_table->get_symbol(i)->get_value();
    stream << std::hex << val;
    std::string hexString = stream.str();
    string output = std::to_string(global_symbol_table->get_symbol(i)->get_id()) + "\t\t" + global_symbol_table->get_symbol(i)->get_name() + "\t\t0x" + hexString +"\t\t"+ bind +"\t\t" + sec +"\n";
    *this->output_file_debug << output;
  }
  *this->output_file_debug << "\nEND_SYMBOL_TABLE\n";
}

void Linker::print_relocation_table(){
  *this->output_file_debug << "\nRELOCATION_TABLE\n";
  *this->output_file_debug << "\nNAME\t\tSECTION\t\tOFFSET\t\tADDEND\n";
  for(int i = 0 ; i < this->global_relocation_table->all_relocations.size() ; ++i){
    std::stringstream stream;
    ulong offs = this->global_relocation_table->all_relocations.at(i)->get_offset();
    stream << std::hex << offs;
    std::string hexString = stream.str();

    std::stringstream stream2;
    ulong addnd = this->global_relocation_table->all_relocations.at(i)->get_addend();
    stream2 << std::hex << addnd;
    std::string hexString2 = stream2.str();

    *this->output_file_debug << this->global_relocation_table->all_relocations.at(i)->get_symbol()->get_name() << "\t\t" << this->global_relocation_table->all_relocations.at(i)->get_section()->get_name() << "\t\t0x" <<  hexString<< "\t\t0x" <<  hexString2;
    *this->output_file_debug << "\n";
  }
   *this->output_file_debug << "\nEND_RELOCATION_TABLE\n";
}

void Linker::print_segment_table(){
  *this->output_file_debug << "\nSEGMENT TABLE\n";
  for(int i = 0 ; i < this->global_segment_table->segments.size(); ++i){
    std::stringstream stream;
    stream << std::hex << this->global_segment_table->segments.at(i)->get_start_address();
    std::string hexString = stream.str();
    *this->output_file_debug <<"0x"<< hexString << "\t";

    for(int j = 0 ; j < this->global_segment_table->segments.at(i)->machine_code.size() ; ++j){
      *this->output_file_debug << this->global_segment_table->segments.at(i)->machine_code.at(j).to_string()<< " ";
    }
    *this->output_file_debug << "\n";
  }
}

void Linker::print_hex(){
  int total_bits = 0;
  int hexWidth = 2;
  std::sort(this->global_segment_table->segments.begin(), this->global_segment_table->segments.end(), [this](const Segment* a, const Segment* b) {return compareSegmentsByAddress(a, b);});
  ulong last_start_addr = this->global_segment_table->segments.empty() ? 0 : this->global_segment_table->segments.at(0)->get_start_address();

  for(int i = 0 ; i < this->global_segment_table->segments.size() ; ++i){
    std::stringstream streamAddr;
    ulong start_addr = this->global_segment_table->segments.at(i)->get_start_address();
    
    //addresses are not aligned
    if(i != 0 && start_addr != last_start_addr + 4){
      total_bits = 0;
    }


    if(total_bits == 0){
      streamAddr << std::hex << std::setw(8) << std::setfill('0') << start_addr;
      string hexAddr = streamAddr.str();
      *this->output_file  << "0x"<<hexAddr<<"\t";
    }

    for(int j = 0 ; j < this->global_segment_table->segments.at(i)->get_size(); ++j){
      std::stringstream stream;
      total_bits+=8;
      stream << std::hex << std::setw(hexWidth) << std::setfill('0')  << this->global_segment_table->segments.at(i)->machine_code.at(j).to_ullong();
      string hexString = stream.str();
      *this->output_file << hexString << "\t";
      if(total_bits >= 64){
        *this->output_file  << "\n";
        total_bits = 0;
      }
    }

    if(i == this->global_segment_table->segments.size() - 1 && total_bits < 64){
      total_bits = 0;
    }

    if(i < this->global_segment_table->segments.size() - 1 && start_addr + 4!= this->global_segment_table->segments.at(i + 1)->get_start_address() && total_bits == 32)
    {
      ulong start_addr_next = this->global_segment_table->segments.at(i+1)->get_start_address();
      if(start_addr_next == 3){}
      bitset<8> binaryChunk("00000000");
      std::stringstream helper;
      for(int k = 0 ; k < 4 ; ++k){
        helper << std::hex << std::setw(hexWidth) << std::setfill('0')  << binaryChunk.to_ullong() << "\t";
      }
      string helperString = helper.str();
      *this->output_file << helperString <<"\n";
    }

    last_start_addr = this->global_segment_table->segments.at(i)->get_start_address();
  }
}

void Linker::change_section_id_unique(){
  for(int i = 0 ; i < this->all_sections.size(); ++i){
    this->all_sections.at(i)->set_id(i);
  }
}

void Linker::change_symbol_id_unique(){
  for(int i = 0 ; i <  this->global_symbol_table->all_symbols.size(); ++i){
    this->global_symbol_table->all_symbols.at(i)->set_id(i);
  }
}

void Linker::resolve_symbols(){
  for(int i = 0 ; i < this->global_symbol_table->all_symbols.size(); ++i){
    this->global_symbol_table->all_symbols.at(i)->set_value(this->global_symbol_table->all_symbols.at(i)->get_value() + this->global_symbol_table->all_symbols.at(i)->get_section()->get_start_address());
    this->global_symbol_table->all_symbols.at(i)->set_id(i);
  }
}

Symbol* Linker::get_symbol_by_name(string n){
  for(int i = 0 ; i < this->global_symbol_table->all_symbols.size(); ++i){
    if(this->global_symbol_table->all_symbols.at(i)->get_name() == n) return this->global_symbol_table->all_symbols.at(i);
  }
  string ret = "Symbol " + n + " does not exist in global symbol table.";
  throw ExceptionAlert(ret);
}

bool Linker::contains_symbol(string n){
  for(int i = 0 ; i < this->global_symbol_table->all_symbols.size(); ++ i){
    if(this->global_symbol_table->all_symbols.at(i)->get_name() == n) return true;
  }
  return false;
}

void Linker::resolve_relocation_symbols(){
  for(int i = 0 ; i < this->global_relocation_table->all_relocations.size(); ++i){
    this->global_relocation_table->all_relocations.at(i)->set_symbol(
      this->get_symbol_by_name(this->global_relocation_table->all_relocations.at(i)->get_name())
    );
  }
}

void Linker::resolve_relocations(){
  for(int i = 0 ; i < this->global_relocation_table->all_relocations.size(); ++i){
    ulong relocation_location = this->global_relocation_table->all_relocations.at(i)->get_offset(); //place in memory where the relocation needs to happen
    ulong relocation_addend = this->global_relocation_table->all_relocations.at(i)->get_addend(); 
    Symbol* that_symbol = this->global_relocation_table->all_relocations.at(i)->get_symbol();
    Segment* that_segment = this->get_segment_by_start_address(relocation_location);
    string symbol_value = this->number_to_binary32_string(that_symbol->get_value() + relocation_addend);

    that_segment->machine_code.clear();

    //push machine code in segment, literal value of the literal 
    for (size_t i = 0; i < symbol_value.size(); i += 8) {
      std::string binaryChunk = symbol_value.substr(i, 8);
      that_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
    }

    std::reverse(that_segment->machine_code.begin(), that_segment->machine_code.end());

  }
}

Symbol* Linker::get_symbol_by_value(uint v){
  for(int i = 0 ; i < this->global_symbol_table->all_symbols.size(); ++i){
    if(this->global_symbol_table->all_symbols.at(i)->get_value() == v) return this->global_symbol_table->all_symbols.at(i);
  }
  throw ExceptionAlert("Could not find symbol with specified value in get_symbol_by_value().");
}

Segment* Linker::get_segment_by_start_address(ulong a){
  for(int i = 0 ; i < this->global_segment_table->segments.size(); ++i){
    if(this->global_segment_table->segments.at(i)->get_start_address() == a) return this->global_segment_table->segments.at(i);
  }
  throw ExceptionAlert("Could not find segment with specified address in get_segment_by_start_address().");
}

string Linker::number_to_binary32_string(uint i){
  std::bitset<32> binary_representation(i);
  string binary_string = binary_representation.to_string();
  return binary_string;
}