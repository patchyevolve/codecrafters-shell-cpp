#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>   // getenv
#include <unistd.h>  // access

std::vector<std::string> builtins = { "exit" , "echo" , "type"};

bool is_Builtin(std::string command){
  for(auto builtin : builtins){
    if(command == builtin){
      return true;
    }
  }
  return false;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true){
    std::cout << "$ ";
    
    std::string cmd;
    std::getline(std::cin , cmd);

    std::vector<std::string> tokens;
    std::string current;

    for (char c : cmd) {
      if (c != ' '){
        current += c;
      }
      else {
        if(!current.empty()){
          tokens.push_back(current);
          current.clear();
        }
      }
    }

    if(!current.empty()){
      tokens.push_back(current);
    }

    if(tokens.empty()) continue;

    if(tokens[0] == "exit"){ 
      break;
    }
    else if ( tokens[0] == "echo"){
      for( size_t i = 1 ; i < tokens.size() ; ++i){
        std::cout << tokens[i];
        if(i + 1 < tokens.size()){
          std::cout << " ";
        }
      }
      std::cout << std::endl;
    }
    else if (tokens[0] == "type"){
      if(tokens.size() < 2){
        std::cout << "type: missing operand\n";
        continue;
      }

      if(is_Builtin(tokens[1])){
        std::cout<< tokens[1] << " is a shell builtin" << std::endl;
      }
      else if (!is_Builtin(tokens[1])){
        //get path
        char* path_env = getenv("PATH");
        if(!path_env){
          std::cout << tokens[1] << ": not found" << std::endl;
          continue;
        }

        //path seperation
        std::vector<std::string> dirs;
        std::string cop;

        for(char *p = path_env; *p != '\0' ; ++p){ //path_env is not a std string
          if( *p != ':'){
            cop += *p;
          }
          else{
            if(!cop.empty()){
              dirs.push_back(cop);
              cop.clear();
            }
          }
        }
        if(!cop.empty()){
          dirs.push_back(cop);
        }

        //path lookup
        bool found = false;

        for(const auto& dir : dirs){
          std::string fullPath = dir + "/" + tokens[1];

          if(access(fullPath.c_str(), F_OK) != 0){
            continue;
          }
          
          if(access(fullPath.c_str(), X_OK) != 0){
            continue;
          }

          std::cout << tokens[1] << " is " << fullPath << std::endl;
          found = true;
          break;
        }

        if(!found){
          std::cout << tokens[1] << ": not found" << std::endl;
        }
      }

      continue;

    }
    else{
        std::cout << tokens[0] << ": command not found" << std::endl ;
    }
  }
  
}
