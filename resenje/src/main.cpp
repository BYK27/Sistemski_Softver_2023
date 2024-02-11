#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
//#include "../inc/assembler.hpp"
#include "assembler.cpp"
#include "linker.cpp"
#include "emulator.cpp"

int main(int argc, const char** argv){

  try{

    //ASSEMBLING
    
    std::ifstream input_file1("../tests/main.s");
    std::ofstream output_file1("output_main.o");
    Assembler* assembler_main = new Assembler(&input_file1, &output_file1);
    

    std::ifstream input_file2("../tests/math.s");
    std::ofstream output_file2("output_math.o");
    Assembler* assembler_math = new Assembler(&input_file2, &output_file2);


    std::ifstream input_file3("../tests/handler.s");
    std::ofstream output_file3("output_handler.o");
    Assembler* assembler_handler = new Assembler(&input_file3, &output_file3);

    std::ifstream input_file4("../tests/isr_terminal.s");
    std::ofstream output_file4("output_isr_terminal.o");
    Assembler* assembler_isr_terminal = new Assembler(&input_file4, &output_file4);

    std::ifstream input_file5("../tests/isr_timer.s");
    std::ofstream output_file5("output_isr_timer.o");
    Assembler* assembler_isr_timer = new Assembler(&input_file5, &output_file5);

    std::ifstream input_file6("../tests/isr_software.s");
    std::ofstream output_file6("output_isr_software.o");
    Assembler* assembler_isr_software = new Assembler(&input_file6, &output_file6);
    

    delete assembler_main;
    delete assembler_math;
    delete assembler_handler;
    delete assembler_isr_software;
    delete assembler_isr_terminal;
    delete assembler_isr_timer;  
    
    //LINKING
    
    
    unordered_map<string, uint32_t> place_arguments = unordered_map<string, uint32_t>();
    place_arguments["math"] = 4026531840;
    place_arguments["my_code"] = 0x40000000;
    vector<ifstream*> input_files;
    std::ofstream output_file_linker("aplication.hex");
    
    std::ifstream input_file_linker1("output_main.o");
    std::ifstream input_file_linker2("output_math.o");
    std::ifstream input_file_linker3("output_handler.o");
    std::ifstream input_file_linker4("output_isr_terminal.o");
    std::ifstream input_file_linker5("output_isr_timer.o");
    std::ifstream input_file_linker6("output_isr_software.o");

    input_files.push_back(&input_file_linker3);
    input_files.push_back(&input_file_linker2);
    input_files.push_back(&input_file_linker1);
    input_files.push_back(&input_file_linker4);
    input_files.push_back(&input_file_linker5);
    input_files.push_back(&input_file_linker6);


    Linker* linker = new Linker(input_files, &output_file_linker, place_arguments);
    linker->print_hex();
    delete linker;
    
    //EMULATION

    ifstream* e = new ifstream("aplication.hex");
    Emulator* emulator = new Emulator(e);
    delete emulator;
    
  }
  catch(ExceptionAlert& e){
    std::cout << e.get_message() << "\n";
  }
  
  return 0;
}