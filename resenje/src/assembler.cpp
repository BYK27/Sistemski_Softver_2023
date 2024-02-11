#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <algorithm>
using namespace std;

#include "../inc/assembler.hpp"

uint Assembler::next_symbol_id = 0;
uint Assembler::next_section_id = 0;

Assembler::Assembler(ifstream* input, ofstream* output){

  std::string helper = "%r1,%r2,shl$5";
  std::string helper1 ;
  std::string helper2 ;
  std::string helper3 ;
  std::string helper4 ;
  std::string helper5 ;
  this->parseRegisters_and_shift(helper, helper1, helper2, helper3, helper4);

  
  this->input_file = input;
  this->output_file = output;
  this->current_line = 1;
  this->current_section = nullptr;

  this->all_sections = vector<Section*>();
  this->all_operations = vector<Operation*>();
  this->relocation_table = new RelocationTable();
  this->symbol_table = new SymbolTable();
  this->segment_table = new SegmentTable();

  this->first_pass();
  this->second_pass();
  this->resolve_local_relocations();
  this->print_section_table();
  this->print_symbol_table();
  this->print_relocation_table();
  this->print_segments_by_section();
  this->check_for_exceptions();

  input->close();
  output->close();

}


Assembler::~Assembler(){
  this->input_file = nullptr;
  this->output_file = nullptr;
  this->current_section = nullptr;

  for(int i = 0 ; i < this->all_sections.size(); ++i){
    delete this->all_sections.at(i);
  }

  // for(int i = 0 ; i < this->all_operations.size(); ++i){
  //   delete this->all_operations.at(i);
  // }

  delete relocation_table;
  delete symbol_table;
  delete segment_table;
}

int Assembler::first_pass(){

  //allocate new, undefined section
  Section* undefined = new Section();
  undefined->set_id(Assembler::next_section_id++);
  undefined->set_name("UND");
  undefined->set_start_address(0);
  this->current_section = undefined;
  this->all_sections.push_back(undefined);

  //allocate new, undefined symbol
  Symbol* undefined_symbol = new Symbol();
  undefined_symbol->set_id(Assembler::next_symbol_id++);
  undefined_symbol->set_section(undefined);
  undefined_symbol->set_name("UND");
  undefined_symbol->set_isSection();
  this->symbol_table->add_symbol(undefined_symbol);

  //parsing start
  bool end = false;
  string line;

  //parse entry file line by line
  while(getline(*this->input_file, line) && !end){
    string cleaned_line = clean_line(line);
    //line is eligible
    if(cleaned_line.length() > 0){
      //line is a DIRECTIVE
      if(cleaned_line.find('.') != std::string::npos){

        size_t position_dot = cleaned_line.find('.');
        cleaned_line.erase(position_dot, 1);    //erase `.`

        size_t position_space = cleaned_line.find(' ');
        string directive = cleaned_line.substr(0, position_space); //a directive name (global, extern, section...)

        cleaned_line.erase(0, position_space + 1);  //list of arguments after 

        position_dot = cleaned_line.find(',');
        while(position_dot != std::string::npos){
          string argument_name = cleaned_line.substr(0, position_dot);
          cleaned_line.erase(0, position_dot + 1);
          cleaned_line = clean_line(cleaned_line);

          if(directive=="global") this->directive_global(argument_name);
          else if(directive=="extern") this->directive_extern(argument_name);
          else if(directive=="section") this->directive_section(argument_name);
          else if(directive=="word") this->directive_word(argument_name);
          else if(directive=="skip") this->directive_skip(argument_name);
          else if(directive=="ascii") this->directive_ascii(argument_name); //not provided in level A
          else if(directive=="equ") this->directive_equ(argument_name); //not provided in level A
          else if(directive=="end") {end = true; break;}
          else throw ExceptionAlert("Unsupported directive in line " + current_line);

          position_dot = cleaned_line.find(',');
          if(position_dot == std::string::npos) break;
        }
        
        cleaned_line = clean_line(cleaned_line);
        string argument_name = cleaned_line;

        if(directive=="global") this->directive_global(argument_name);
        else if(directive=="extern") this->directive_extern(argument_name);
        else if(directive=="section") this->directive_section(argument_name);
        else if(directive=="word") this->directive_word(argument_name);
        else if(directive=="skip") this->directive_skip(argument_name);
        else if(directive=="ascii") this->directive_ascii(argument_name); //not provided in level A
        else if(directive=="equ") this->directive_equ(argument_name); //not provided in level A
        else if(directive=="end") {end = true; break;}
        else throw ExceptionAlert("Unsupported directive in line " + current_line);

        cleaned_line = "";
        
      }
      //line is a LABEL
      else if(cleaned_line.find(':') != std::string::npos){

        cleaned_line = clean_line(cleaned_line);
        size_t position = cleaned_line.find(':');

        string name = cleaned_line.substr(0, position);

        //defined symbol
        if(this->symbol_table->contains_symbol(name)){
          Symbol* that_symbol = this->symbol_table->get_symbol_by_name(name);
          that_symbol->set_section(this->current_section);
          that_symbol->set_defined();
          that_symbol->set_value(this->current_section->get_location_counter());
        }
        //undefined symbol
        else{
          Symbol* new_symbol = new Symbol();
          new_symbol->set_id(Assembler::next_symbol_id++);
          new_symbol->set_section(current_section);
          new_symbol->set_name(name);
          new_symbol->isSymbol();
          new_symbol->set_defined();
          new_symbol->set_value(this->current_section->get_location_counter());
          this->symbol_table->add_symbol(new_symbol);
        }
      }
      //line is an OPERATION   
      else{
        size_t position = cleaned_line.find(' ');
        string code_name = cleaned_line.substr(0, position);
        cleaned_line = cleaned_line.erase(0, position);
        int code = this->operations.at(code_name);


        string gpr1 = "", gpr2 = "", operand = "", csr = "";
        string shift_operation = "";
        
        switch (code)
        {
          //no register instructions
        case 0: //halt
        case 1: //int
        case 4: //ret
        {
          if(code == 1 && cleaned_line.find('{') != string::npos)
          {
            clean_line(cleaned_line);
            cleaned_line.erase(0, cleaned_line.find('{') + 1);
            cleaned_line.erase(cleaned_line.find('}'), cleaned_line.find('}'));

            std::vector<string> array_of_registers;

            size_t position_comma = cleaned_line.find(',');
            size_t position_percent = cleaned_line.find('%');

            while(position_comma != string::npos)
            {
              cleaned_line = clean_line(cleaned_line);
              cleaned_line.erase(0, position_percent+1);
              std::string that_register = cleaned_line.substr(0, position_comma - 1);
              cleaned_line.erase(0, position_comma + 1);
              that_register = clean_line(that_register);
              array_of_registers.push_back(that_register);
              position_comma = cleaned_line.find(',');
              size_t position_percent = cleaned_line.find('%');
              
            }
            cleaned_line = clean_line(cleaned_line);
            cleaned_line.erase(0, cleaned_line.find('%') + 1);
            array_of_registers.push_back(cleaned_line);
            
            for(int u = 0 ; u < array_of_registers.size(); ++u)
            {

              std::cout << array_of_registers.at(u) << "|\n";
              Operation* new_operation = new Operation(9, "push");

              new_operation->set_gpr1(array_of_registers.at(u));

              this->all_operations.push_back(new_operation);

              Segment* new_segment = new Segment();
              new_segment->set_start_address(current_section->get_location_counter());
              new_segment->set_defined();
              new_segment->set_contains_operation();
              new_segment->set_operation(new_operation);
              new_segment->set_section(this->current_section);

              this->segment_table->add_segment(new_segment);
              this->current_section->increment_location_counter_by(4);
            }

          }

          //actual INT operation
          Operation* new_operation = new Operation(code, code_name);
          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_defined();
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(4);

          break;
        }
        case 2: //iret
        {
          Operation* new_operation = new Operation(code, code_name);
          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_defined();
          new_segment->set_contains_operation();
          new_segment->set_size(8);
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(8);

          break;
        }
          //one register instructions
        case 9: //push %gpr1
        case 10: //pop %gpr1
        case 16: //not %gpr1
        {  

          if(code == 9 && cleaned_line.find('{') != string::npos)
          {
            std::vector<std::string> array_of_registers;
            //erase '{'
            cleaned_line.erase(cleaned_line.find('{'), 1);  
            //find comma
            size_t position_comma = cleaned_line.find(',');
            //go until the last 
            while(position_comma != string::npos)
            {
              std::string that_register = cleaned_line.substr(0, position_comma);
              cleaned_line.erase(0, position_comma + 1);
              that_register = clean_line(that_register);
              array_of_registers.push_back(that_register);
              position_comma = cleaned_line.find(',');
            }
            cleaned_line = clean_line(cleaned_line);
            position_comma = cleaned_line.find('}');
            array_of_registers.push_back(cleaned_line.substr(0, position_comma));


            //sort array
            std::sort(array_of_registers.begin(), array_of_registers.end());

            for(int u = 0 ; u < array_of_registers.size(); ++u)
            {
              Operation* new_operation = new Operation(code, code_name);

              new_operation->set_gpr1(array_of_registers.at(u));

              this->all_operations.push_back(new_operation);

              Segment* new_segment = new Segment();
              new_segment->set_start_address(current_section->get_location_counter());
              new_segment->set_defined();
              new_segment->set_contains_operation();
              new_segment->set_operation(new_operation);
              new_segment->set_section(this->current_section);

              this->segment_table->add_segment(new_segment);
              this->current_section->increment_location_counter_by(4);
            }
          }
          else
          {
            Operation* new_operation = new Operation(code, code_name);
            cleaned_line = clean_line(cleaned_line);
            size_t position_percent = cleaned_line.find('%');
            gpr1 = cleaned_line.erase(position_percent, 1);   //erase '%'

            new_operation->set_gpr1(gpr1);

            this->all_operations.push_back(new_operation);

            Segment* new_segment = new Segment();
            new_segment->set_start_address(current_section->get_location_counter());
            new_segment->set_defined();
            new_segment->set_contains_operation();
            new_segment->set_operation(new_operation);
            new_segment->set_section(this->current_section);

            this->segment_table->add_segment(new_segment);
            this->current_section->increment_location_counter_by(4);
          }
          

          break;
        }
        case 3: //call operand
        case 5: //jmp operand
        {
          //operand is always literal/symbol value
          Operation* new_operation = new Operation(code, code_name);

          cleaned_line = clean_line(cleaned_line);
          new_operation->set_operand(cleaned_line);

          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          if(code == 5) new_segment->set_size(8);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(new_segment->get_size());

          //allocate a segment for literal pool
          this->directive_word(cleaned_line);
          break;
        }

        case 11: //xchg %gpr1, %gpr2
        case 12: //add %gpr1, %gpr2
        case 13: //sub %gpr1, %gpr2
        case 14: //mul %gpr1, %gpr2
        case 15: //div %gpr1, %gpr2
        {
          Operation* new_operation = new Operation(code, code_name);

          cleaned_line = clean_line(cleaned_line);  //removes blanco spaces before %gpr1, %gpr2

          //normal add instruction
          if(cleaned_line.find("sh") == string::npos)
          {
            this->parseRegisters(cleaned_line, gpr1, gpr2);

            new_operation->set_gpr1(gpr1);
            new_operation->set_gpr2(gpr2);
          }
          else
          {
            this->parseRegisters_and_shift(cleaned_line, gpr1, gpr2, shift_operation, operand);

            new_operation->set_gpr1(gpr1);
            new_operation->set_gpr2(gpr2);
            new_operation->set_operand(operand);
            new_operation->set_shift_operation(shift_operation);
          }
          
          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_defined();
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(4);


          break;
          }

        case 6: //beq %gpr1, %gpr2, operand
        case 7: //bne %gpr1, %gpr2, operand
        case 8: //bgt %gpr1, %gpr2, operand
        {

          Operation* new_operation = new Operation(code, code_name);

          cleaned_line = clean_line(cleaned_line);  //removes blanco spaces before %gpr1, %gpr2

          //gpr1
          position = cleaned_line.find(','); 
          size_t position_percent = cleaned_line.find('%');
          gpr1 = cleaned_line.substr(position_percent + 1, position - position_percent - 1); 
          cleaned_line.erase(0, position + 1);  //erases everything up until the `,`
          
          cleaned_line = clean_line(cleaned_line);  //remove blanco spaces

          //gpr2
          position = cleaned_line.find(',');
          position_percent = cleaned_line.find('%');
          gpr2 = cleaned_line.substr(position_percent + 1, position - position_percent - 1); 
          cleaned_line.erase(0, position + 1);  //erases everything up until the `,`

          cleaned_line = clean_line(cleaned_line);  //remove blanco spaces

          //operand
          position = cleaned_line.find(" ");
          operand = cleaned_line.substr(0, position);

          new_operation->set_gpr1(gpr1);
          new_operation->set_gpr2(gpr2);
          new_operation->set_operand(operand);

          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);
          new_segment->set_section(this->current_section);
          new_segment->set_size(8);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(new_segment->get_size());

          this->directive_word(operand);

          break;
        }
        
        case 22: //ld operand, %gpr1
        {
          Operation* new_operation = new Operation(code, code_name);
          uint32_t literal_value = 0;

          cleaned_line = clean_line(cleaned_line);

          //operand
          position = cleaned_line.find(',');
          operand = cleaned_line.substr(0, position);

          //gpr1
          cleaned_line = cleaned_line.erase(0, position);
          cleaned_line = clean_line(cleaned_line);
          position = cleaned_line.find('%');
          cleaned_line.erase(0, position+1);
          position = cleaned_line.find(" ");
          gpr1 = cleaned_line.substr(0, position);

          new_operation->set_operand(operand);  //contains reg2 in case of register_symbol or register_literal
          new_operation->set_gpr1(gpr1);

          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(new_segment->get_size());

          Symbol* new_symbol;

          //find operand type and set appropriate registers
          if(operand.find('$') != std::string::npos){
            size_t position_temp = operand.find('$');
            operand.erase(position_temp, 1);  //erase `$` symbol to get the literal/symbol
            operand = clean_line(operand);

            if(this->is_binary(operand) || this->is_hex(operand) || this->is_number(operand)){
              if(this->is_number(operand)) {
                literal_value = this->string_to_int(operand);
                }
              else if(this->is_binary(operand)) {
                literal_value = this->binary_to_int(operand);
                }
              else if(this->is_hex(operand)) {
                literal_value = this->hex_to_int(operand);
              }
              else throw ExceptionAlert("Invalid literal value in " + this->current_line);

              if(literal_value < 2048){
                new_operation->set_operand_type(Operation::IMMEDIATE);
                new_operation->set_operand(operand);
              }
              else{
                new_operation->set_operand_type(Operation::LITERAL);
                new_operation->set_operand(operand);
              }
            }
            else{
              new_operation->set_operand(operand);
              new_operation->set_operand_type(Operation::SYMBOL);
            }
          }
          else if(operand.find('+') != std::string::npos){
            size_t position_percent = operand.find('%');
            size_t position_space = operand.find(' ', position_percent);
            string register_name = operand.substr(position_percent + 1, position_space - position_percent - 1);


            size_t position_after_plus = operand.find('+');
            operand.erase(0, position_after_plus + 1);
            cleaned_line = clean_line(cleaned_line);
            position_after_plus = operand.find("]");
            operand.erase(position_after_plus, 1);
            operand = clean_line(operand);

            //two register ld operation
            if(operand.find('%') != string::npos)
            {
              operand.erase(0, operand.find('%')+1);
              operand = operand.substr(0, operand.find('\t'));
              
              new_operation->set_operand_type(Operation::REG_REG);
              
            }
            else
            {
              if(this->is_binary(operand) || this->is_number(operand) || this->is_hex(operand)){
              if(this->is_number(operand)) literal_value = this->string_to_int(operand);
              else if(this->is_binary(operand)) literal_value = this->binary_to_int(operand);
              else if(this->is_hex(operand)) literal_value = this->hex_to_int(operand);
              else throw ExceptionAlert("Invalid literal value in " + this->current_line);

              //literal can be built into the instruction, no need for relocation
              if(literal_value < 2048){
                new_operation->set_operand_type(Operation::REG_LITERAL_MEM);
                
              }
              else throw ExceptionAlert("Literal exceeds 12bits in line " + this->current_line);
              }
              else{
                new_operation->set_operand_type(Operation::REG_SYMBOL_MEM);
              }
            }

            new_operation->set_gpr2(register_name);
            new_operation->set_operand(operand);
            
          }
          else if(operand.find('[') != std::string::npos){
            size_t position_bracket = operand.find(']');
            size_t position_percent = operand.find('%');
            string register_name = operand.substr(position_percent + 1, position_bracket - position_percent - 1);
            new_operation->set_operand(register_name);
            new_operation->set_operand_type(Operation::REGISTER_MEM);
          }
          else if(operand.find('%')  != std::string::npos){
            size_t position_percent = operand.find('%');
            string register_name = operand.erase(0, position_percent);
            new_operation->set_operand(register_name);
            new_operation->set_operand_type(Operation::REGISTER);
          }
          else{
            //operand is a literal
            if(this->is_binary(operand) || this->is_hex(operand) || this->is_number(operand)){
            int literal_value = 0;
            if(this->is_number(operand)) literal_value = this->string_to_int(operand);
            else if(this->is_binary(operand)) literal_value = this->binary_to_int(operand);
            else if(this->is_hex(operand)) literal_value = this->hex_to_int(operand);

            //literal value can be directly coded in the machine code
            if(literal_value < 2048){
              new_operation->set_operand_type(Operation::IMMEDIATE_MEM);
              new_operation->set_operand(operand);

            }
            else{
              new_operation->set_operand_type(Operation::LITERAL_MEM);
              new_segment->set_size(12);
              this->current_section->increment_location_counter_by(8);
            }
          }
          //operand is a symbol
          else
          {
            new_operation->set_operand_type(Operation::SYMBOL_MEM);
            new_segment->set_size(12);
            this->current_section->increment_location_counter_by(8);
          }
          }

          switch(new_operation->get_operand_type()){
            case Operation::IMMEDIATE:
            case Operation::IMMEDIATE_MEM:
            case Operation::REGISTER:
            case Operation::REGISTER_MEM:
            case Operation::REG_LITERAL_MEM:
            case Operation::REG_REG:
            {
              new_segment->set_defined();
              break;
            }
            case Operation::LITERAL:
            case Operation::LITERAL_MEM:
            case Operation::SYMBOL:
            case Operation::SYMBOL_MEM:
            {
              this->directive_word(new_operation->get_operand()); //allocates a segment in literal pool
              break;
            }
            case Operation::REG_SYMBOL_MEM:
            {
              break;
            }           
            
            default:
            {
              throw ExceptionAlert("Unsupported operation in " + this->current_line);
              break;
            }
            
          }

          break;
        }
        case 23: //st %gpr1, operand
        {
          
          Operation* new_operation = new Operation(code, code_name);
          uint32_t literal_value = 0;

          cleaned_line = clean_line(cleaned_line);

          //gpr1
          position = cleaned_line.find(','); 
          size_t position_percent = cleaned_line.find('%');
          gpr1 = cleaned_line.substr(position_percent + 1, position - position_percent - 1); 
          cleaned_line.erase(0, position + 1);  //erases everything up until the `,`
          
          cleaned_line = clean_line(cleaned_line);  //remove blanco spaces

          //operand
          cleaned_line = clean_line(cleaned_line);
          position = cleaned_line.find(" ");
          operand = cleaned_line;

          new_operation->set_operand(operand);  //contains reg2 in case of register_symbol or register_literal
          new_operation->set_gpr1(gpr1);

          this->all_operations.push_back(new_operation);

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(new_segment->get_size());

          Symbol* new_symbol;
          //find operand type and set appropriate registers
          if(operand.find('$') != std::string::npos){
            size_t position_temp = operand.find('$');
            operand.erase(position_temp, 1);  //erase `$` symbol to get the literal/symbol
            operand = clean_line(operand);

            if(this->is_binary(operand) || this->is_hex(operand) || this->is_number(operand)){
              if(this->is_number(operand)) {
                literal_value = this->string_to_int(operand);
                }
              else if(this->is_binary(operand)) {
                literal_value = this->binary_to_int(operand);
                }
              else if(this->is_hex(operand)) {

                literal_value = this->hex_to_int(operand);
              }
              else throw ExceptionAlert("Invalid literal value in " + this->current_line);


              if(literal_value < 2048){
                new_operation->set_operand_type(Operation::IMMEDIATE);
                new_operation->set_operand(operand);
              }
              else{
                new_operation->set_operand_type(Operation::LITERAL);
                new_operation->set_operand(operand);
              }
            }
            else{
              new_operation->set_operand(operand);
              new_operation->set_operand_type(Operation::SYMBOL);
            }
          }
          else if(operand.find('+') != std::string::npos){
            
            size_t position_percent = operand.find('%');
            size_t position_space = operand.find(' ', position_percent);
            string register_name = operand.substr(position_percent + 1, position_space - position_percent - 1);

            size_t position_after_plus = operand.find('+');
            operand.erase(0, position_after_plus + 1);
            cleaned_line = clean_line(cleaned_line);
            position_after_plus = operand.find("]");
            operand.erase(position_after_plus, 1);
            operand = clean_line(operand);

            
            //two register st operation
            if(operand.find('%') != string::npos)
            {
              
              operand.erase(0, operand.find('%')+1);
              operand = operand.substr(0, operand.find('\t'));
              operand = clean_line(operand);
              new_operation->set_operand_type(Operation::REG_REG);
              
            }
            else
            {
              if(this->is_binary(operand) || this->is_number(operand) || this->is_hex(operand)){
                if(this->is_number(operand)) literal_value = this->string_to_int(operand);
                else if(this->is_binary(operand)) literal_value = this->binary_to_int(operand);
                else if(this->is_hex(operand)) literal_value = this->hex_to_int(operand);
                else throw ExceptionAlert("Invalid literal value in " + this->current_line);

                //literal can be built into the instruction, no need for relocation
                if(literal_value < 2048){
                  new_operation->set_operand_type(Operation::REG_LITERAL_MEM);
                }
                else throw ExceptionAlert("Literal exceeds 12bits in line " + this->current_line);
              }
              else{
                new_operation->set_operand_type(Operation::REG_SYMBOL_MEM);
              }
            }

            new_operation->set_gpr2(register_name);
            new_operation->set_operand(operand);

            
          }
          else if(operand.find('[') != std::string::npos){
            
            size_t position_bracket = operand.find(']');
            size_t position_percent = operand.find('%');
            string register_name = operand.substr(position_percent + 1, position_bracket - position_percent - 1);
            new_operation->set_operand(register_name);
            new_operation->set_operand_type(Operation::REGISTER_MEM);
          }
          else if(operand.find('%')  != std::string::npos){
            size_t position_percent = operand.find('%');
            string register_name = operand.erase(0, position_percent);
            new_operation->set_operand(register_name);
            new_operation->set_operand_type(Operation::REGISTER);
          }
          else{
            //operand is a literal
            if(this->is_binary(operand) || this->is_hex(operand) || this->is_number(operand)){
            int literal_value = 0;
            if(this->is_number(operand)) literal_value = this->string_to_int(operand);
            else if(this->is_binary(operand)) literal_value = this->binary_to_int(operand);
            else if(this->is_hex(operand)) literal_value = this->hex_to_int(operand);

            //literal value can be directly coded in the machine code
            if(literal_value < 2048){
              new_operation->set_operand_type(Operation::IMMEDIATE_MEM);
              new_operation->set_operand(operand);
            }
            else{
              new_operation->set_operand_type(Operation::LITERAL_MEM);
            }
          }
          //operand is a symbol
          else
          {
            new_operation->set_operand_type(Operation::SYMBOL_MEM);
            
          }
          }

          switch(new_operation->get_operand_type()){
            case Operation::IMMEDIATE:
            case Operation::IMMEDIATE_MEM:
            case Operation::REGISTER:
            case Operation::REGISTER_MEM:
            case Operation::REG_LITERAL_MEM:
            case Operation::REG_REG:
            {
              new_segment->set_defined();
              break;
            }
            case Operation::LITERAL:
            case Operation::LITERAL_MEM:
            case Operation::SYMBOL:
            case Operation::SYMBOL_MEM:
            {
              new_segment->set_size(8);
              this->current_section->increment_location_counter_by(4);
              this->directive_word(new_operation->get_operand()); //allocates a segment in literal pool
              break;
            }
            case Operation::REG_SYMBOL_MEM:
            {
              break;
            }           
            
            default:
            {
              throw ExceptionAlert("Unsupported operation in " + this->current_line);
              break;
            }
            
          }
          break;
        }
        case 24: //csrrd %csr, %gpr
        {
          Operation* new_operation = new Operation(code, code_name);

          cleaned_line = clean_line(cleaned_line);  //removes blanco spaces before %gpr1, %gpr2

          //csr
          position = cleaned_line.find(','); 
          size_t position_percent = cleaned_line.find('%');
          csr = cleaned_line.substr(position_percent + 1, position - position_percent - 1); 
          cleaned_line.erase(0, position + 1);  //erases everything up until the `,`
          
          cleaned_line = clean_line(cleaned_line);  //remove blanco spaces

          //gpr
          position_percent = cleaned_line.find('%');
          gpr1 = cleaned_line.erase(position_percent, 1);

          new_operation->set_csr(csr);
          new_operation->set_gpr1(gpr1);

          this->all_operations.push_back(new_operation);


          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_defined();
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(4);


          break;
        }
        case 25: //csrwr %gpr, %csr
        {
          Operation* new_operation = new Operation(code, code_name);

          cleaned_line = clean_line(cleaned_line);  //removes blanco spaces before %gpr1, %gpr2

          //gpr
          position = cleaned_line.find(','); 
          size_t position_percent = cleaned_line.find('%');
          gpr1 = cleaned_line.substr(position_percent + 1, position - position_percent - 1); 
          cleaned_line.erase(0, position + 1);  //erases everything up until the `,`
          
          cleaned_line = clean_line(cleaned_line);  //remove blanco spaces

          //csr
          position_percent = cleaned_line.find('%');
          csr = cleaned_line.erase(position_percent, 1);

          new_operation->set_csr(csr);
          new_operation->set_gpr1(gpr1);

          this->all_operations.push_back(new_operation);
          

          Segment* new_segment = new Segment();
          new_segment->set_start_address(current_section->get_location_counter());
          new_segment->set_defined();
          new_segment->set_contains_operation();
          new_segment->set_operation(new_operation);
          new_segment->set_section(this->current_section);

          this->segment_table->add_segment(new_segment);
          this->current_section->increment_location_counter_by(4);

          break;
        }
        default:
          throw ExceptionAlert("Undefined operation at "+ this->current_line);
          break;
        }
        
      }
      
    }
    ++this->current_line;
  }
  return 0;
}


//SECOND PASS
int Assembler::second_pass(){
  for(int line = 0 ; line < this->segment_table->segments.size(); ++line){
    //line is an operation, it generates MACHINE CODE
    if(segment_table->segments.at(line)->get_operation() != nullptr){
      Operation* operation_at_line = segment_table->segments.at(line)->get_operation();
      switch (operation_at_line->get_code())
      {
      case 0: //HALT
      {
        this->operation_halt(operation_at_line);
        break;
      }

      case 1: //INT
      {
        this->operation_int(operation_at_line);
        break;
      }

      case 2: //IRET
      {
        this->operation_iret(operation_at_line);
        break;
      }

      case 3: //CALL operand - relocation needed
      {
        this->operation_call(operation_at_line);
        break;
      }

      case 4: //RET
      {
        this->operation_ret(operation_at_line);
        break;
      }

      case 5: //JMP operand - relocation needed
      {
        this->operation_jmp(operation_at_line);
        break;
      }

      case 6: //BEQ %gpr1, %gpr2, operand - relocation needed
      {
        this->operation_beq(operation_at_line);
        break;
      }

      case 7: //BNE %gpr1, %gpr2, operand - relocation needed
      {
        this->operation_bne(operation_at_line);
        break;
      }

      case 8: //BGT %gpr1, %gpr2, operand - relocation needed
      {
        this->operation_bgt(operation_at_line);
        break;
      }

      case 9: //PUSH %gpr1
      {
        this->operation_push(operation_at_line);
        break;
      }

      case 10: //POP %gpr1
      {
        this->operation_pop(operation_at_line);
        break;
      }

      case 11: //XCHG %gpr1, %gpr2
      {
        this->operation_xchg(operation_at_line);
        break;
      }

      case 12: //ADD %gpr1, %gpr2
      {
        if(operation_at_line->get_shift_operation() == "")this->operation_add(operation_at_line);
        else this->operation_add_special(operation_at_line);
        break;
      }

      case 13: //SUB %gpr1, %gpr2
      {
        this->operation_sub(operation_at_line);
        break;
      }

      case 14: //MUL %gpr1, %gpr2
      {
        this->operation_mul(operation_at_line);
        break;
      }

      case 15: //DIV %gpr1, %gpr2
      {
        this->operation_div(operation_at_line);
        break;
      }

      case 16: //NOT %gpr1
      {
        this->operation_not(operation_at_line);
        break;
      }

      case 17: //AND %gpr1, %gpr2
      {
        this->operation_and(operation_at_line);
        break;
      }

      case 18: //OR %gpr1, %gpr2
      {
        this->operation_or(operation_at_line);
        break;
      }

      case 19: //XOR %gpr1, %gpr2
      {
        this->operation_xor(operation_at_line);
        break;
      }

      case 20: //SHL %gpr1, %gpr2
      {
        this->operation_shl(operation_at_line);
        break;
      }

      case 21: //SHR %gpr1, %gpr2
      {
        this->operation_shr(operation_at_line);
        break;
      }

      case 22: //LD operand, %gpr1 - relocation needed
      {
        this->operation_ld(operation_at_line);
        break;
      }

      case 23: //ST %gpr1, operand - relocation needed
      {
        this->operation_st(operation_at_line);
        break;
      }

      case 24: //CSRRD %csr, %gpr1
      {
        this->operation_csrrd(operation_at_line);
        break;
      }

      case 25: //CSRWR %gpr1, %csr
      {
        this->operation_csrwr(operation_at_line);
        break;
      }
      default:
        throw ExceptionAlert("Invalid operation in Assembler::second_pass()");
        break;
      }

      //put the operation code in the segment
      //IMPORTANT: possible bad destribution due to little endian format
      std::bitset<32> operation_machine_code = operation_at_line->machine_code;

      for(int b = 0 ; b < 4 ; ++b){
        std::bitset<8> segment(operation_machine_code.to_string(), b * 8, 8);
        this->segment_table->segments.at(line)->machine_code.push_back(segment);
      }

      //LD instruction with indirect memory addressing needs two sections with the same code
      if(operation_at_line->get_code() == 22){
        //gpr <= mem[gpr]
        
        if(operation_at_line->get_operand_type() == Operation::SYMBOL_MEM || operation_at_line->get_operand_type() == Operation::LITERAL_MEM){
          std::bitset<8> segment1("10010010");
          std::bitset<8> segment2("00000000");  //to be replaced with gpr gpr
          std::bitset<8> segment3("00000000");
          std::bitset<8> segment4("00000000");
          int registerNo1 = this->registers.at(operation_at_line->get_gpr1());
          string registerNo1ValueHelper = std::bitset<4>(registerNo1).to_string();
          for (int i = 0; i < 4; ++i) {
            if (registerNo1ValueHelper[i] == '1') {
              segment2.set(3 - i); // Set the corresponding bit to 1
              segment2.set(7 - i); // Set the corresponding bit to 1
            } else {
              segment2.reset(3 - i); // Set the corresponding bit to 0
              segment2.reset(7 - i); // Set the corresponding bit to 0
            }
          }
          this->segment_table->segments.at(line)->machine_code.push_back(segment1);
          this->segment_table->segments.at(line)->machine_code.push_back(segment2);
          this->segment_table->segments.at(line)->machine_code.push_back(segment3);
          this->segment_table->segments.at(line)->machine_code.push_back(segment4);

          std::bitset<8> segment11("10010001");
          std::bitset<8> segment21("11111111");  
          std::bitset<8> segment31("00000000");
          std::bitset<8> segment41("00000100");

          this->segment_table->segments.at(line)->machine_code.push_back(segment11);
          this->segment_table->segments.at(line)->machine_code.push_back(segment21);
          this->segment_table->segments.at(line)->machine_code.push_back(segment31);
          this->segment_table->segments.at(line)->machine_code.push_back(segment41);

        }
      }
      //ST
      else if(operation_at_line->get_code() == 23){
        if(operation_at_line->get_operand_type() == Operation::SYMBOL_MEM || operation_at_line->get_operand_type() == Operation::LITERAL_MEM ||
        operation_at_line->get_operand_type() == Operation::SYMBOL || operation_at_line->get_operand_type() == Operation::LITERAL){
          std::bitset<8> segment1("10010001");
          std::bitset<8> segment2("11111111");  
          std::bitset<8> segment3("00000000");
          std::bitset<8> segment4("00000100");

          this->segment_table->segments.at(line)->machine_code.push_back(segment1);
          this->segment_table->segments.at(line)->machine_code.push_back(segment2);
          this->segment_table->segments.at(line)->machine_code.push_back(segment3);
          this->segment_table->segments.at(line)->machine_code.push_back(segment4);
        }
        
      }
      //JMP, BEQ, BNE, BGT
      else if(operation_at_line->get_code() == 5 || operation_at_line->get_code() == 6 || operation_at_line->get_code() == 7 || operation_at_line->get_code() == 8){
        
        std::bitset<8> segment1("10010001");
        std::bitset<8> segment2("11111111");  
        std::bitset<8> segment3("00000000");
        std::bitset<8> segment4("00000100");

        this->segment_table->segments.at(line)->machine_code.push_back(segment1);
        this->segment_table->segments.at(line)->machine_code.push_back(segment2);
        this->segment_table->segments.at(line)->machine_code.push_back(segment3);
        this->segment_table->segments.at(line)->machine_code.push_back(segment4);
        
      }

      //IRET instruction is the only one that has an 8 byte segment
      if(operation_at_line->get_code() == 2){
        std::bitset<32> operation_machine_code = bitset<32>("10010111000011100000000000000100");
        //std::bitset<32> operation_machine_code = bitset<32>("1001 0111  - 0000 1110 - 0000 0000 - 0000 0100");

        for(int b = 0 ; b < 4 ; ++b){
          std::bitset<8> segment(operation_machine_code.to_string(), b * 8, 8);
          this->segment_table->segments.at(line)->machine_code.push_back(segment);
        } 
      }

      
    }

    //line is a symbol that needs realocating
    else if(segment_table->segments.at(line)->isSymbol()){
      Relocation* new_relocation = new Relocation();
      new_relocation->set_symbol(segment_table->segments.at(line)->get_symbol());
      new_relocation->set_section(segment_table->segments.at(line)->get_section());
      new_relocation->set_offset(segment_table->segments.at(line)->get_start_address());
      this->relocation_table->add_relocation(new_relocation);
      segment_table->segments.at(line)->set_relocation(new_relocation);
    }
    else{
      //do nothing, literal and skip have built in machine code
      
    }
  }
  return 1;
}

//removes starting blanco spaces
string Assembler::clean_line(string l){
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

string Assembler::get_word_line(string l){
  string new_word = "";

  int i = 0;
  while(l[i] != ' ' && l[i] != '\t' && l[i] != '\0' && l[i] != '\n'){
    new_word += l[i];
  }
  return new_word;
}

string Assembler::remove_blanco_spaces(string l){
  int i = 0;
  string new_word = "";
  while(l[i] != '\0'){
    if(l[i] != ' ' || l[i] != '\t'){
      new_word += l[i];
    }
    ++i;
  }
  return new_word;
}


//introduces new GLOBAL symbols in the symbol table
void Assembler::directive_global(string s){
  string expression = s;
  Symbol* new_symbol = new Symbol();
  new_symbol->set_id(Assembler::next_symbol_id++);
  new_symbol->set_value(0); //value not initialized
  new_symbol->set_name(expression);
  new_symbol->set_section(this->current_section);
  new_symbol->set_isSymbol();
  new_symbol->set_binding(Symbol::GLOBAL);     
  this->symbol_table->add_symbol(new_symbol);
}

void Assembler::directive_extern(string s){

  string expression = s;

  if(this->symbol_table->contains_symbol(expression)){
    throw ExceptionAlert("Multiple symbol definition.");
  }
  Symbol* new_symbol = new Symbol();
  new_symbol->set_id(Assembler::next_symbol_id++);
  new_symbol->set_value(0); //value not initialized
  new_symbol->set_name(expression);
  new_symbol->set_section(this->current_section);
  new_symbol->set_isSymbol();
  new_symbol->set_extern();
  this->symbol_table->add_symbol(new_symbol);

}

void Assembler::directive_section(string s){
  string command = s;

  if(Assembler::contains_section(command)){
    throw ExceptionAlert("Multiple section definition.");
  }

  Section* new_section = new Section();
  new_section->set_id(Assembler::next_section_id++);
  new_section->set_name(command);
  new_section->set_start_address(0);
  this->current_section = new_section;
  this->all_sections.push_back(new_section);

  Symbol* new_symbol = new Symbol();
  new_symbol->set_id(Assembler::next_symbol_id++);
  new_symbol->set_section(current_section);
  new_symbol->set_name(command);
  new_symbol->set_isSection();
  this->symbol_table->add_symbol(new_symbol);
  
}

void Assembler::directive_word(string s){
  string expression = clean_line(s);
  //parameter is a literal, it can be built into the machine code od the segment
  if(this->is_number(expression) || this->is_binary(expression) || this->is_hex(expression)){

    int literal_value = 0;
    if(this->is_number(expression)) literal_value = this->string_to_int(expression);
    else if(this->is_binary(expression)) literal_value = this->binary_to_int(expression);
    else if(this->is_hex(expression)) literal_value = this->hex_to_int(expression);

    //value can be directly coded coded
    Segment* new_segment = new Segment();
    new_segment->set_start_address(this->current_section->get_location_counter());
    new_segment->set_contains_literal();
    new_segment->set_section(this->current_section);
    new_segment->set_defined();

    string binaryString = this->number_to_binary32_string(literal_value);
    
    //push machine code in segment, literal value of the literal 
    for (size_t i = 0; i < binaryString.size(); i += 8) {
      std::string binaryChunk = binaryString.substr(i, 8);
      new_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
    }

    std::reverse(new_segment->machine_code.begin(), new_segment->machine_code.end());

    this->segment_table->add_segment(new_segment);
    this->current_section->increment_location_counter_by(new_segment->get_size());

  }
  //parameter is a symbol
  else{
    string symbol_name = expression;
    Symbol* new_symbol;
    //symbol not in symbol table, make a new symbol
    if(!this->symbol_table->contains_symbol(symbol_name)){
      new_symbol = new Symbol();
      new_symbol->set_id(Assembler::next_symbol_id++);
      new_symbol->set_section(current_section);
      new_symbol->set_name(expression);
      new_symbol->isSymbol();
      this->symbol_table->add_symbol(new_symbol);
    }
    else{
      new_symbol = symbol_table->get_symbol_by_name(symbol_name);
    }

    Segment* new_segment = new Segment();
    new_segment->set_start_address(this->current_section->get_location_counter());
    new_segment->set_contains_symbol();
    new_segment->set_section(this->current_section);
    new_segment->set_symbol(new_symbol);

    //push machine code in segment that will have a relocation later
    for (size_t i = 0; i < 4; ++i) {
      std::string binaryChunk = "00000000";
      new_segment->machine_code.push_back(std::bitset<8>(binaryChunk));
    }
    
    this->segment_table->add_segment(new_segment);
    this->current_section->increment_location_counter_by(new_segment->get_size());
  }
}
 
void Assembler::directive_skip(string s){
  string command = s;

  Segment* new_segment = new Segment();
  new_segment->set_size(this->string_to_int(command));
  new_segment->set_start_address(this->current_section->get_location_counter());
  this->current_section->increment_location_counter_by(this->string_to_int(command));
  new_segment->set_defined();
  new_segment->set_skip();
  new_segment->set_section(this->current_section);

  //push empty machine code into the segment
  for(int i = 0 ; i < this->string_to_int(command) ; ++i){
    new_segment->machine_code.push_back(std::bitset<8>("00000000"));
  }

  this->segment_table->add_segment(new_segment);
}

void Assembler::directive_ascii(string s){

}

void Assembler::directive_equ(string s){
  throw ExceptionAlert("Zadato za nivo B.");
}

void Assembler::directive_end(string s){
  
}


bool Assembler::contains_section(string name){
  for(int i = 0 ; i < Assembler::all_sections.size(); ++i){
    if(all_sections.at(i)->get_name() == name) return true;
  }
  return false;
}

bool Assembler::is_number(string& s){
  // string::const_iterator it = s.begin();
  // while (it != s.end() && std::isdigit(*it)) ++it;
  // return !s.empty() && it == s.end();

  for (char c : s) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}

bool Assembler::is_hex(string s){
  if(s.find("0x") != std::string::npos) return true;
  else return false;
}

bool Assembler::is_binary(string s){
  // if(s.at(0) == '0' && (s.at(1) == 'b' || s.at(1) == 'B')) return true;
  // else return false;
  if(s.find("0b") != std::string::npos) return true;
  else return false;

}

uint Assembler::hex_to_int(string s){
  // size_t position_x = s.find('x');
  // s.erase(0, position_x);
  // uint value;

  // std::stringstream stream;
  // stream << s;
  // stream >> hex >> value;

  unsigned long decimalValue = std::stoul(s, nullptr, 0); // Convert hexadecimal string to decimal value
  uint32_t uintValue = static_cast<uint32_t>(decimalValue); // Convert the decimal value to a uint32_t

  return uintValue;
}

uint Assembler::binary_to_int(string s){
  return std::bitset<32>(s).to_ulong();  
}

uint Assembler::string_to_int(string s){
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

string Assembler::number_to_binary32_string(uint i){
  std::bitset<32> binary_representation(i);
  string binary_string = binary_representation.to_string();
  return binary_string;
}



//OPERATIONS
void Assembler::operation_halt(Operation* p){
  p->machine_code = (std::bitset<32>("00000000000000000000000000000000"));
}

void Assembler::operation_int(Operation* p){
  p->machine_code = (std::bitset<32>("00010000000000000000000000000000"));
}


void Assembler::operation_iret(Operation* p){
  p->machine_code = (std::bitset<32>("10010011111111100000000000000100"));
  //p->machine_code =(std::bitset<32>      ("1001 0111 - 0000 1110 - 0000 0000 - 0000 0100"));
}


//relocation
void Assembler::operation_call(Operation* p){
  p->machine_code = (std::bitset<32>("00100001111100000000000000000000"));
  //p->machine_code = (std::bitset<32>("0010 0001 - 1111 0000 - 0000 0000 - 0000 0000));
  //p->masks = (std::bitset<32>("0000 0000 - 0000 0000 - 0000 1111 - 1111 1111"));
}

void Assembler::operation_ret(Operation* p){
  p->machine_code = (std::bitset<32>("10010011111111100000000000000100"));
}

//relocation
void Assembler::operation_jmp(Operation* p){
  p->machine_code = (std::bitset<32>("00111000111100000000000000000100"));
  //p->machine_code = (std::bitset<32>("0011 1000 -  1111 0000 - 0000 0000 - 0000 0100")); -> jmp relative
}

//relocation
void Assembler::operation_beq(Operation* p){

  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  
  p->machine_code = (std::bitset<32>("00111001111100000000000000000100"));
  //p->machine_code = (std::bitset<32>("0011 1001 - 1111 gpr1 - gpr2 0000 - 0000 0100"));

  std::string register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
 }

//relocation
void Assembler::operation_bne(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());

  p->machine_code = (std::bitset<32>("00111010111100000000000000000100"));
  //p->machine_code = (std::bitset<32>("0011 1010 - 1111 gpr1 - gpr2 ???? - ???? ????"));

  std::string register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

//relocation
void Assembler::operation_bgt(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());

  p->machine_code = (std::bitset<32>("00111011111100000000000000000100"));
  //p->machine_code = (std::bitset<32>("0011 1011 - 1111 gpr1 - gpr2 ???? - ???? ????"));

  std::string register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_push(Operation* p){
  int registerNo = this->registers.at(p->get_gpr1());
  p->machine_code = (std::bitset<32>("10000001111000000000111111111100"));
  //p->machine_code = (std::bitset<32>("1000 0001 - 1110 0000 - 0000 1111 - 1111 1100"));
  std::string register_binary_value = std::bitset<4>(registerNo).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_pop(Operation* p){

  int registerNo = this->registers.at(p->get_gpr1());
  p->machine_code = (std::bitset<32>("10010011000011100000000000000100"));

  std::string register_binary_value = std::bitset<4>(registerNo).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_xchg(Operation* p){
  //masks are 0, no need to modify
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01000000000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(17 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(17 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_add(Operation* p){
  
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  
  p->machine_code = (std::bitset<32>("01010000000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_add_special(Operation* p){
  
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());

  int is_right = (p->get_shift_operation() == "shr" ? 1 : 0);
  int value = this->string_to_int(p->get_operand());
  
  p->machine_code = (std::bitset<32>("01010100000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

  if(is_right) p->machine_code.set(8);

  register_binary_value = std::bitset<8>(value).to_string();
  for(int i = 0; i < 8 ; ++i)
  {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(7 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(7 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_sub(Operation* p){
  //masks are 0, no need to modify
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01010001000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_mul(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01010010000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_div(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01010011000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }

  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_not(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  p->machine_code = (std::bitset<32>("01100000000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_and(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01100001000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

}

void Assembler::operation_or(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01100010000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

}

void Assembler::operation_xor(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01100011000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

}

void Assembler::operation_shl(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01110000000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

}

void Assembler::operation_shr(Operation* p){
  int registerNo1 = this->registers.at(p->get_gpr1());
  int registerNo2 = this->registers.at(p->get_gpr2());
  p->machine_code = (std::bitset<32>("01110001000000000000000000000000"));

  std::string register_binary_value = std::bitset<4>(registerNo2).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNo1).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
  }

}

void Assembler::operation_ld(Operation* p){
  // int registerNo1 = this->registers.at(p->get_gpr1());
  // int registerNo2 = this->registers.at(p->get_gpr2());
  // int registerCsr = this->status_registers.at(p->get_csr());


  string registerNo1Value = "";
  string registerNo2Value = "";
  string registerCsrValue = "";
  string operandNoValue = p->get_operand();
  switch (p->get_operand_type())
  {

  case Operation::IMMEDIATE : //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10010001000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0001 - gpr1 0000 - 0000 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<12>(this->string_to_int(operandNoValue)).to_string();

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    //machine code finished during assembling
    for(int i = 0 ; i < 12 ; ++i){
      if (operandNoValue[i] == '1') {
          p->machine_code.set(11 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(11 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::IMMEDIATE_MEM : //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10010010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0010 - gpr1 0000 - 0000 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<12>(this->string_to_int(operandNoValue)).to_string();

    for (int i = 0; i < 4; ++i) {
    if (registerNo1Value[i] == '1') {
      p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
    }

    //machine code finished during assembling
    for(int i = 0 ; i < 12 ; ++i){
      if (operandNoValue[i] == '1') {
          p->machine_code.set(11 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(11 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::SYMBOL:
  case Operation::LITERAL:
  
  {
    p->machine_code = (std::bitset<32>("10010011000011110000000000000100"));
    //p->machine_code = (std::bitset<32>("1001 0011 - gpr1 1111 - 0000 0000 - 0000 0100"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    for (int i = 0; i < 4; ++i) {
    if (registerNo1Value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
    }
    break;
  }
  case Operation::SYMBOL_MEM:   
  case Operation::LITERAL_MEM:  
  {
    //reg <= mem[pc+4] because the next 4bytes are allocated for the ld helper 
    p->machine_code = (std::bitset<32>("10010010000011110000000000001000"));
    //p->machine_code = (std::bitset<32>("1001 0010 - gpr1 1111 - 0000 0000 - 0000 1000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    for (int i = 0; i < 4; ++i) {
    if (registerNo1Value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
    }
    break;
  }
  
  case Operation::REGISTER: //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10010001000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0001 - gpr1 reg2 - 0000 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::REGISTER_MEM: //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10010010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0010 - gpr1 reg2 - 0000 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::REG_SYMBOL_MEM:
  {
    //exceptions - symbol must be defined turing asembling and have a 12 bit value
    if(!this->symbol_table->get_symbol_by_name(operandNoValue)->isDefined())throw ExceptionAlert("Symbol value not defined during assembling.");
    //if(this->symbol_table->get_symbol_by_name(operandNoValue)->get_value() >= 2048) throw new ExceptionAlert("Symbol value not defined during assembling.");

    p->machine_code = (std::bitset<32>("10010010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0010 - gpr1 gpr2 - 0000 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    int registerNo2 = this->registers.at(p->get_gpr2());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    registerNo2Value = std::bitset<4>(registerNo2).to_string();

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (registerNo2Value[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }

    break;
  }
  case Operation::REG_LITERAL_MEM:  //no relocation needed
  {
    //COME
    //literal is in range
    int literal_value = 0;
    if(this->is_number(operandNoValue)) literal_value = this->string_to_int(operandNoValue);
    else if(this->is_binary(operandNoValue)) literal_value = this->binary_to_int(operandNoValue);
    else if(this->is_hex(operandNoValue)) literal_value = this->hex_to_int(operandNoValue);

    if(literal_value < 2048){
      p->machine_code = (std::bitset<32>("10010010000000000000000000000000"));
      //p->machine_code = (std::bitset<32>("1001 0010 - gpr1 gpr2 - 0000 ???? - ???? ????"));
      int registerNo1 = this->registers.at(p->get_gpr1());
      int registerNo2 = this->registers.at(p->get_gpr2());
      registerNo1Value = std::bitset<4>(registerNo1).to_string();
      registerNo2Value = std::bitset<4>(registerNo2).to_string();
      operandNoValue = std::bitset<12>(literal_value).to_string(); //register hiding in `operand` field

      for (int i = 0; i < 4; ++i) {
        if (registerNo1Value[i] == '1') {
            p->machine_code.set(23 - i); // Set the corresponding bit to 1
        } else {
            p->machine_code.reset(23 - i); // Set the corresponding bit to 0
        }
      }
      for (int i = 0; i < 4; ++i) {
        if (registerNo2Value[i] == '1') {
            p->machine_code.set(19 - i); // Set the corresponding bit to 1
        } else {
            p->machine_code.reset(19 - i); // Set the corresponding bit to 0
        }
      }

      for (int i = 0; i < 12; ++i) {
        if (operandNoValue[i] == '1') {
            p->machine_code.set(11 - i); // Set the corresponding bit to 1
        } else {
            p->machine_code.reset( 11 -i); // Set the corresponding bit to 0
        }
      }
    }
    else throw ExceptionAlert("Literal value too big in operation.");

    break;
  }

  case Operation::REG_REG:
  {
     p->machine_code = (std::bitset<32>("10011000000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 1000 - gpr1 reg2 - reg3 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    int registerNo2 = this->registers.at(p->get_gpr2());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    registerNo2Value = std::bitset<4>(registerNo2).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (registerNo2Value[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  default:
  {
    throw ExceptionAlert("Illegal addressing mode.");
    break;
  }
  }
}

void Assembler::operation_st(Operation* p){
  // int registerNo1 = this->registers.at(p->get_gpr1());
  // int registerNo2 = this->registers.at(p->get_gpr2());
  // int registerCsr = this->status_registers.at(p->get_csr());

  string registerNo1Value = "";
  string registerNo2Value = "";
  string registerCsrValue = "";
  string operandNoValue = p->get_operand();
  switch (p->get_operand_type())
  {

  case Operation::IMMEDIATE : //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10000000000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0000 - 0000 0000 - gpr1 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<12>(this->string_to_int(operandNoValue)).to_string();

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    //machine code finished during assembling
    for(int i = 0 ; i < 12 ; ++i){
      if (operandNoValue[i] == '1') {
          p->machine_code.set(11 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(11 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::IMMEDIATE_MEM : //no relocation needed
  {
    p->machine_code = (std::bitset<32>("10000010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0010 - 0000 0000 - gpr1 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<12>(this->string_to_int(operandNoValue)).to_string();

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    //machine code finished during assembling
    for(int i = 0 ; i < 12 ; ++i){
      if (operandNoValue[i] == '1') {
          p->machine_code.set(11 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(11 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::SYMBOL:
  case Operation::LITERAL:
  {
    p->machine_code = (std::bitset<32>("10000000111100000000000000000100"));
    //p->machine_code = (std::bitset<32>("1000 0000 - 1111 0000 - gpr1 0000 - 0000 0100"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    for (int i = 0; i < 4; ++i) {
    if (registerNo1Value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
    }
    break;
  }
  case Operation::SYMBOL_MEM:
  case Operation::LITERAL_MEM:
  {
    p->machine_code = (std::bitset<32>("10000010111100000000000000000100"));
    //p->machine_code = (std::bitset<32>("1000 0010 - 1111 0000 - gpr1 0000 - 0000 0100"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    for (int i = 0; i < 4; ++i) {
    if (registerNo1Value[i] == '1') {
        p->machine_code.set(15 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(15 - i); // Set the corresponding bit to 0
    }
    }
    break;
  }
  case Operation::REGISTER: //no relocation needed
  {

    /*
    p->machine_code = (std::bitset<32>("10000000000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0000 - gpr2 0000 - gpr1 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }
    break;
    */

    p->machine_code = (std::bitset<32>("10010001000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1001 0001 - gpr1 reg2 - 0000 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::REGISTER_MEM: //no relocation needed
  {
    /*
    p->machine_code = (std::bitset<32>("10000010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0010 - gpr2 0000 - gpr1 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }
    break;
    */
    p->machine_code = (std::bitset<32>("10000000000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0000 - gpr2 0000 - gpr1 0000 - 0000 0000"));

    int registerNo1 = this->registers.at(p->get_gpr1());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field
    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }
    break;

  }
  case Operation::REG_SYMBOL_MEM:
  {
    //exceptions - symbol must be defined turing asembling and have a 12 bit value
    if(!this->symbol_table->get_symbol_by_name(operandNoValue)->isDefined())throw ExceptionAlert("Symbol value not defined during assembling.");
    //if(this->symbol_table->get_symbol_by_name(operandNoValue)->get_value() < -2048 || this->symbol_table->get_symbol_by_name(operandNoValue)->get_value() >= 2048) throw new ExceptionAlert("Symbol value not defined during assembling.");

    p->machine_code = (std::bitset<32>("10000010000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0010 - gpr2 0000 - gpr1 ???? - ???? ????"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    int registerNo2 = this->registers.at(p->get_gpr2());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    registerNo2Value = std::bitset<4>(registerNo2).to_string();
    operandNoValue = std::bitset<4>(this->string_to_int(operandNoValue)).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (registerNo2Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

    for(int i = 0 ; i < 12 ; ++i){
      if (operandNoValue[i] == '1') {
          p->machine_code.set(11 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(11 - i); // Set the corresponding bit to 0
      }
    }
    break;
  }
  case Operation::REG_LITERAL_MEM:  //no relocation needed
  {

    //TODO: add down limit >= - 2048
    //literal is in range
    if( this->string_to_int(operandNoValue) < 2048 ){
      p->machine_code = (std::bitset<32>("10000000000000000000000000000000"));
      //p->machine_code = (std::bitset<32>("1000 0000 - gpr2 0000 - gpr1 ???? - ???? ????"));

      int registerNo1 = this->registers.at(p->get_gpr1());
      int registerNo2 = this->registers.at(p->get_gpr2());
      registerNo1Value = std::bitset<4>(registerNo1).to_string();
      registerNo2Value = std::bitset<4>(registerNo2).to_string();
      
      operandNoValue = std::bitset<12>(this->string_to_int(operandNoValue)).to_string(); //register hiding in `operand` field

      for (int i = 0; i < 4; ++i) {
        if (registerNo1Value[i] == '1') {
            p->machine_code.set(15 - i); // Set the corresponding bit to 1
        } else {
            p->machine_code.reset(15 - i); // Set the corresponding bit to 0
        }
      }

      for (int i = 0; i < 4; ++i) {
      if (registerNo2Value[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }

      for (int i = 0; i < 12; ++i) {
        if (operandNoValue[i] == '1') {
            p->machine_code.set(11 - i); // Set the corresponding bit to 1
        } else {
            p->machine_code.reset(11 - i); // Set the corresponding bit to 0
        }
      }
    }
    else throw ExceptionAlert("Literal value too big in operation.");

    break;
  }
  case Operation::REG_REG:
  {
    p->machine_code = (std::bitset<32>("10000011000000000000000000000000"));
    //p->machine_code = (std::bitset<32>("1000 0011 - gpr2 gpr3 - gpr1 0000 - 0000 0000"));
    int registerNo1 = this->registers.at(p->get_gpr1());
    int registerNo2 = this->registers.at(p->get_gpr2());
    registerNo1Value = std::bitset<4>(registerNo1).to_string();
    registerNo2Value = std::bitset<4>(registerNo2).to_string();
    operandNoValue = std::bitset<4>(this->registers.at(p->get_operand())).to_string(); //register hiding in `operand` field

    for (int i = 0; i < 4; ++i) {
      if (registerNo1Value[i] == '1') {
          p->machine_code.set(15 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(15 - i); // Set the corresponding bit to 0
      }
    }

    for (int i = 0; i < 4; ++i) {
      if (registerNo2Value[i] == '1') {
          p->machine_code.set(19 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(19 - i); // Set the corresponding bit to 0
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (operandNoValue[i] == '1') {
          p->machine_code.set(23 - i); // Set the corresponding bit to 1
      } else {
          p->machine_code.reset(23 - i); // Set the corresponding bit to 0
      }
    }
    break;
    break;
  }
  default:
  {
    throw ExceptionAlert("Illegal addressing mode.");
    break;
  }
  }
}

void Assembler::operation_csrrd(Operation* p){
  int registerNo = this->registers.at(p->get_gpr1());
  int registerNoStatus= this->status_registers.at(p->get_csr());
  p->machine_code = (std::bitset<32>("10010000000000000000000000000000"));
  
  std::string register_binary_value = std::bitset<4>(registerNo).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNoStatus).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(19 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
}

void Assembler::operation_csrwr(Operation* p){
  int registerNo = this->registers.at(p->get_gpr1());
  int registerNoStatus= this->status_registers.at(p->get_csr());
  p->machine_code = (std::bitset<32>("10010100000000000000000000000000"));
  std::string register_binary_value = std::bitset<4>(registerNo).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(19 -  i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(19 - i); // Set the corresponding bit to 0
    }
  }
  register_binary_value = std::bitset<4>(registerNoStatus).to_string();
  for (int i = 0; i < 4; ++i) {
    if (register_binary_value[i] == '1') {
        p->machine_code.set(23 - i); // Set the corresponding bit to 1
    } else {
        p->machine_code.reset(23 - i); // Set the corresponding bit to 0
    }
  }
}

Section* Assembler::get_section_by_name(string n){
  for(int i = 0 ; i < this->all_sections.size(); ++i){
    if(this->all_sections.at(i)->get_name() == n){
      return this->all_sections.at(i);
    }
  }
  throw ExceptionAlert("Section by name not found");
  return nullptr;
}


Segment* Assembler::add_segment_for_operation(Operation* new_operation){

  this->all_operations.push_back(new_operation);

  Segment* new_segment = new Segment();
  new_segment->set_start_address(current_section->get_location_counter());
  new_segment->set_defined();
  new_segment->set_contains_operation();
  new_segment->set_operation(new_operation);
  new_segment->set_section(this->current_section);

  this->segment_table->add_segment(new_segment);
  this->current_section->increment_location_counter_by(4);

  return new_segment;
}

void Assembler::print_section_table(){
  *this->output_file << "\nSECTION_TABLE\n";
  *this->output_file << "\nID\t\tNAME\t\tADDRESS\t\tSIZE\n";
  for(int i = 0 ; i < this->all_sections.size(); ++i){
    std::stringstream stream;
    std::stringstream stream2;
    ulong addrs = this->all_sections.at(i)->get_start_address();
    ulong loc = this->all_sections.at(i)->get_location_counter();
    stream << std::hex << addrs;
    stream2 << std::hex << loc;
    std::string hexStringA = stream.str();
    std::string hexStringL = stream2.str();
    *this->output_file << std::to_string(this->all_sections.at(i)->get_id())<<"\t\t"<< this->all_sections.at(i)->get_name() <<"\t\t0x" << hexStringA << "\t\t0x" << hexStringL << "\n";
  }
  *this->output_file << "\nEND_SECTION_TABLE\n";
}

void Assembler::print_symbol_table(){
  *this->output_file << "\nSYMBOL_TABLE\n";
  *this->output_file << "\nID\t\tNAME\t\tVALUE\t\tBINDING\t\tSECTION\n";
  string bind = "";
  string sec = "";
  for(int i = 0 ; i < this->symbol_table->get_size() ; ++i){
    bind = this->symbol_table->get_symbol(i)->get_binding() == Symbol::GLOBAL ? "GLOBAL" : "LOCAL";
    sec = this->symbol_table->get_symbol(i)->get_section()->get_name();
    std::stringstream stream;
    ulong val = this->symbol_table->get_symbol(i)->get_value();
    stream << std::hex << val;
    std::string hexString = stream.str();
    string output = std::to_string(symbol_table->get_symbol(i)->get_id()) + "\t\t" + symbol_table->get_symbol(i)->get_name() + "\t\t0x" + hexString +"\t\t"+ bind +"\t\t" + sec  +"\n";
    *this->output_file << output;
  }
  *this->output_file << "\nEND_SYMBOL_TABLE\n";
}

void Assembler::print_segments(){
  *this->output_file << "\nSEGMENT TABLE\n";
  for(int i = 0 ; i < this->segment_table->segments.size(); ++i){
    

    std::stringstream stream;
    stream << std::hex << this->segment_table->segments.at(i)->get_start_address();
    std::string hexString = stream.str();
    *this->output_file << hexString << "\t";

    for(int j = 0 ; j < this->segment_table->segments.at(i)->machine_code.size() ; ++j){
      *this->output_file << this->segment_table->segments.at(i)->machine_code.at(j).to_string().c_str() << " ";

    }
    *this->output_file << "\n";
  }
}

void Assembler::print_segments_by_section(){
  //prints in little endian format
  *this->output_file << "\nSEGMENT_TABLE\n";

  for(int i = 0 ; i < this->all_sections.size(); ++i){
    *this->output_file << "\nSECTION " << this->all_sections.at(i)->get_name() << "\n";

    for(int j = 0 ; j < this->segment_table->segments.size(); ++j){
      //segment is in the section
      if(this->segment_table->segments.at(j)->get_section() != this->all_sections.at(i)){}
      else{
        for(int l = 0 ; l < this->segment_table->segments.at(j)->machine_code.size()  ; ++l){
          if(l%4 == 0){
            std::stringstream stream;
            ulong addrs = this->segment_table->segments.at(j)->get_start_address();
            addrs += l;
            stream << std::hex << addrs;
            std::string hexString = stream.str();
            *this->output_file<< "\n" <<"0x"<< hexString << "\t";
          }
          *this->output_file << this->segment_table->segments.at(j)->machine_code.at(l).to_string() << " ";
        }
      }
    }
    *this->output_file << "\n";
  }
   *this->output_file << "\nEND_SEGMENT_TABLE\n";
}

void Assembler::print_relocation_table(){
  *this->output_file << "\nRELOCATION_TABLE\n";
  *this->output_file << "\nNAME\t\tSECTION\t\tOFFSET\t\tADDEND\n";
  for(int i = 0 ; i < this->relocation_table->all_relocations.size() ; ++i){
    std::stringstream stream;
    ulong offs = this->relocation_table->all_relocations.at(i)->get_offset();
    stream << std::hex << offs;
    std::string hexString = stream.str();

    std::stringstream stream2;
    ulong addnd = this->relocation_table->all_relocations.at(i)->get_addend();
    stream2 << std::hex << addnd;
    std::string hexString2 = stream2.str();

    *this->output_file << this->relocation_table->all_relocations.at(i)->get_symbol()->get_name() << "\t\t" << this->relocation_table->all_relocations.at(i)->get_section()->get_name() << "\t\t0x" <<  hexString << "\t\t0x" << hexString2;
    *this->output_file << "\n";
  }
   *this->output_file << "\nEND_RELOCATION_TABLE\n";
}

void Assembler::check_for_exceptions(){
  for(int i = 0 ; i < this->symbol_table->all_symbols.size(); ++i){
    if(!this->symbol_table->all_symbols.at(i)->get_extern() && !this->symbol_table->all_symbols.at(i)->isDefined() && !this->symbol_table->all_symbols.at(i)->isSection()) throw ExceptionAlert("Symbol undefined.");
  }
}
void Assembler::resolve_local_relocations(){
  for(int i = 0; i < this->relocation_table->all_relocations.size(); ++ i){
    //local binding are just start of section + value of symbol as addend
    if(this->relocation_table->all_relocations.at(i)->get_symbol()->get_binding() == Symbol::LOCAL && !this->relocation_table->all_relocations.at(i)->get_symbol()->get_extern()){
      this->relocation_table->all_relocations.at(i)->set_addend(this->relocation_table->all_relocations.at(i)->get_symbol()->get_value());
      this->relocation_table->all_relocations.at(i)->set_symbol(this->symbol_table->get_symbol_by_name(this->relocation_table->all_relocations.at(i)->get_symbol()->get_section()->get_name()));
    }
  }
}

void Assembler::parseRegisters(const std::string& registers, std::string& sourceRegister, std::string& destinationRegister) {
    std::istringstream ss(registers);

    // Extract source register
    if (std::getline(ss, sourceRegister, ',')) {
        size_t position_percent = sourceRegister.find('%');
        if (position_percent != std::string::npos) {
            sourceRegister.erase(0, position_percent + 1);  // Remove "%"
        } else {
            // Handle error: % not found in source register
            std::cerr << "Error: % not found in source register" << std::endl;
            return;
        }
    } else {
        // Handle error: Comma not found
        std::cerr << "Error: Comma not found" << std::endl;
        return;
    }

    // Extract destination register
    if (!std::getline(ss, destinationRegister, ',')) {
        // Handle error: No destination register found
        std::cerr << "Error: Destination register not found" << std::endl;
        return;
    }

    // Remove leading whitespaces
    destinationRegister.erase(0, destinationRegister.find_first_not_of(" \t"));

    // Remove trailing whitespaces
    destinationRegister.erase(destinationRegister.find_last_not_of(" \t") + 1);

    size_t position_percent_dest = destinationRegister.find('%');
    if (position_percent_dest != std::string::npos) {
        destinationRegister.erase(0, position_percent_dest + 1);  // Remove "%"
    } else {
        // Handle error: % not found in destination register
        std::cerr << "Error: % not found in destination register" << std::endl;
        return;
    }
}

void Assembler::parseRegisters_and_shift(const std::string& registers, std::string& sourceRegister, std::string& destinationRegister,std::string& operation, std::string& operand) {
    
    
    std::istringstream ss(registers);

    //%r1,%r2,shl$4

    // Extract source register
    if (std::getline(ss, sourceRegister, ',')) {
        size_t position_percent = sourceRegister.find('%');
        if (position_percent != std::string::npos) {
            sourceRegister.erase(0, position_percent + 1);  // Remove "%"
        } else {
            // Handle error: % not found in source register
            std::cerr << "Error: % not found in source register" << std::endl;
            return;
        }
    } else {
        // Handle error: Comma not found
        std::cerr << "Error: Comma not found" << std::endl;
        return;
    }

    // Extract destination register
    if (!std::getline(ss, destinationRegister, ',')) {
        // Handle error: No destination register found
        std::cerr << "Error: Destination register not found" << std::endl;
        return;
    }


    size_t position_percent_dest = destinationRegister.find('%');
    if (position_percent_dest != std::string::npos) {
        destinationRegister.erase(0, position_percent_dest + 1);  // Remove "%"
    } else {
        // Handle error: % not found in destination register
        std::cerr << "Error: % not found in destination register" << std::endl;
        return;
    }

    std::getline(ss, operation, '$');
    std::getline(ss, operand, '\t');

    // Remove leading whitespaces
    operand.erase(0, operand.find_first_not_of(" \t"));
    // Remove trailing whitespaces
    operand.erase(operand.find_last_not_of(" \t") + 1);

    // Remove leading whitespaces
    operation.erase(0, operation.find_first_not_of(" \t"));
    // Remove trailing whitespaces
    operation.erase(operation.find_last_not_of(" \t") + 1);

}