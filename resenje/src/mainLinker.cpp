#include "linker.cpp"

int main(int argc, const char** argv) {


  try
  {
    unordered_map<string, uint32_t> place_arguments = unordered_map<string, uint32_t>();
    vector<ifstream*> input_files;
    ofstream* output_file;
    bool hexFound = false, oFound = false;
    //skip filename
    for (int i = 1; i < argc; ++i) 
    {
      // -HEX
      if (std::string(argv[i]) == "-hex") 
      {
        if(hexFound) throw ExceptionAlert("-hex command specified twice.");
        hexFound = true;
      }

      // -PLACE
      else if (std::string(argv[i]).find("-place") != std::string::npos) 
      {
        string arg = std::string(argv[i]);
        size_t position = arg.find("=");
        arg.erase(0, position + 1);
        position = arg.find("@");

        string section_name = arg.substr(0, position);
        arg.erase(0, position + 1);
        
        unsigned long decimalValue = std::stoul(arg, nullptr, 0); // Convert hexadecimal string to decimal value
        uint32_t section_place = static_cast<uint32_t>(decimalValue); // Convert the decimal value to a uint32_t


        place_arguments[section_name] = section_place;
      }

      // -O 
      else if (std::string(argv[i]) == "-o") 
      {
        if(oFound) throw ExceptionAlert("-o command specified twice.");
        if (i + 1 < argc) 
        { 
          oFound = true;
          std::string outputFileName = argv[i + 1];
          ++i;
          output_file = new ofstream(outputFileName);
        } 
        else throw  ExceptionAlert("Unsupported output filetype.");
      }
      // output.o 
      else
      {
        string file = argv[i];
        if(file.find(".o") == std::string::npos) throw ExceptionAlert("Unsupported output filetype.");
        std::ifstream* input_file = new ifstream(file);
        input_files.push_back(input_file);
      }
    }
    Linker linker = Linker(input_files, output_file, place_arguments);
    linker.print_hex();
    output_file->close();
  }
  catch(ExceptionAlert& e) {
    std::cout<<e.get_message()<<std::endl;
  }
  return 0;
}