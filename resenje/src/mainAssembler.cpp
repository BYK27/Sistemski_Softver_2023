#include "assembler.cpp"

int main(int argc, const char** argv) {

  // argv[0] = "ASSEMBLER"
  // argv[1] = "-o"
  // argv[2] = "main.o"
  // argv[3] = "./tests/main.s"
  // argv[4] = NULL

  try
  {
    if (argc < 4) throw new ExceptionAlert("Not enough assembler arguments.");
    
    string file = argv[3];
    if(file.find(".s") == std::string::npos) throw new ExceptionAlert("Unsupported input filetype.");

    file = argv[2];
    if(file.find(".o") == std::string::npos) throw new ExceptionAlert("Unsupported output filetype.");

    std::ifstream input_file(argv[3]);
    std::ofstream output_file(argv[2]);

    Assembler assembler = Assembler(&input_file, &output_file);
  }

  catch(ExceptionAlert& e) 
  {
    std::cout<<e.get_message()<<std::endl;
    return -1;
  }
  return 0;
}