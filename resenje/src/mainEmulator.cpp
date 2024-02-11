#include "emulator.cpp"

int main(int argc, const char** argv) {

try
{
  if (argc != 2) throw new ExceptionAlert("Insufficient assembler arguments.");
  
  std::string filename = argv[1];

  if(filename.find(".hex") == std::string::npos) throw new ExceptionAlert("Unsupported input filetype.");

  ifstream* file = new ifstream(filename);

  Emulator emulator = Emulator(file);
}
  catch(ExceptionAlert& e) {
    std::cout<<e.get_message()<<std::endl;
  }
  return 0;
}
