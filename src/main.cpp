#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true){
    std::cout << "$ ";
    std::string cmd;
    std::getline(std::cin , cmd);
    if(cmd == "exit"){ break;}
    else{
      std::cout << cmd << " : command not found" << std::endl ;
    }
  }
  
}
