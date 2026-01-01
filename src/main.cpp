#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>   // getenv
#include <unistd.h>  // access
#include <sys/wait.h>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<std::string> builtins = { "exit" , "echo" , "type", "pwd", "cd"};

bool is_Builtin(std::string command){
  for(const auto& builtin : builtins){
    if(command == builtin){
      return true;
    }
  }
  return false;
}


std::vector<std::string> tokenizer(std::string cmd){

    std::vector<std::string> tokens;
    std::string current;
    
    bool in_quotes = false;
    bool escape = false;
    char quote_char = 0;


    for (size_t i = 0 ; i < cmd.size() ; i++){  //loop is changed to interating because in range loop its not possible to peek
        char c = cmd[i];

        //outside quotes : backslash escapes anything
        if(!in_quotes && escape){
            current += c;
            escape = false;
            continue;
        }

        if ( !in_quotes && c == '\\'){
            escape = true;
            continue;
        }

        ///inside double quote
        if( in_quotes && quote_char == '"'){

          if(c == '\\'){
            if( i + 1 < cmd.size()){
                char next = cmd[i+1];
                if ( next == '"' || next == '\\'){
                    current += next;
                    i++;
                }
                else{
                    current += '\\';
                }
            }
            else {
                current += '\\';
            }

            continue;
          }

          if (c == '"'){
            in_quotes = false;
            continue;
          }

          current += c;
          continue;
        }

        // inside single quote
        if(in_quotes && quote_char == '\''){
            if (c == quote_char){
                in_quotes = false;
            }
            else{
                current += c;
            }
            continue;
        }

        //opening quote
        if (c == '"' || c == '\'' ){
            in_quotes = true;
            quote_char = c;
            continue;
        }

        // whitespace
        if (isspace(static_cast<unsigned char>(c))){
            if(!current.empty()){
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        //normal character
        current += c;

    }

    //trailing backslash outside quotes
    if (escape) {
        current += '\\';
    }

    if (in_quotes){
      std::cerr <<  "error : unclosed quotes"<< std::endl;
      return {};
    }

    if(!current.empty()){
      tokens.push_back(current);
    }

    return tokens;
} 

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true){
    
    // intitial
    std::cout << "$ ";
    

    //main command prompt
    std::string cmd;
    std::getline(std::cin , cmd);

    std::vector<std::string> tokens;

    tokens = tokenizer(cmd);

    //empty token edge case
    if(tokens.empty()) continue;

    // main command loop
    if(is_Builtin(tokens[0])){

      // exit command high priority check
      if(tokens[0] == "exit"){ 
        break;
      }

      else if(tokens[0] == "cd"){


        //resolving the target
        std::string target;
        fs::path old_pwd = fs::current_path();


        if (tokens.size() == 1){

          char* home = getenv("HOME");
          if(!home){
            std::cerr << "cd: HOME not set" << std::endl;
            continue;
          }
          target = home;

        }
        else if (tokens[1] == "-"){

          char* old = getenv("OLDPWD");

          if(!old){
            std::cerr << "cd: OLDPWD not set" << std::endl;
            continue;
          }
          target = old;
          
          std::cout << target << std::endl;
        }
        else{
          target = tokens[1];

          if(!target.empty() && target[0] == '~'){

            char* home = getenv("HOME");
            
            if(!home){
              std::cerr << "cd: HOME not set" << std::endl;
              continue;
            }

            if(target.size() == 1 ){
              target = home;
            }
            else if( target[1] == '/'){
              target = std::string(home) + target.substr(1);
            }

          }

        }

        

        //changing the directory
        if(chdir(target.c_str()) != 0){
          std::cerr << "cd: " << target << ": No such file or directory" << std::endl;
          continue;
        }

        //update the enviorment
        fs::path new_pwd = fs::current_path();
        setenv("OLDPWD", old_pwd.string().c_str(), 1);
        setenv("PWD", new_pwd.string().c_str(), 1);

      }

      else if(tokens[0] == "pwd"){
        try {
          fs::path currentPath = fs::current_path();
          std::cout << currentPath.string() <<std::endl;
        }
        catch ( const fs::filesystem_error& e) {
          std::cerr << "filesystem error : " << e.what() << std::endl;
        }
      }

      // if builtin is echo used for printing the string in shell
      else if ( tokens[0] == "echo"){
        for( size_t i = 1 ; i < tokens.size() ; ++i){
          std::cout << tokens[i];
          if(i + 1 < tokens.size()){
            std::cout << " ";
          }
        }
        std::cout << std::endl;
      }

      // if builtin is type used for checking the info and path {if not builtin}
      else if (tokens[0] == "type"){
        
        //if no commad is provide after the type edge case
        if(tokens.size() < 2){
          std::cout << "type: missing operand\n";
          continue;
        }

        // if the command after type are builtin then just showing they are built in
        if(is_Builtin(tokens[1])){
          std::cout<< tokens[1] << " is a shell builtin" << std::endl;
        }

        else{
          
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

    }

    //the case for executing the non builting commands for external programs
    else{
      
      //building args from tokens
      std::vector<char*> argv;

      for(auto& t : tokens){
        argv.push_back(const_cast<char*>(t.c_str()));
      }
      argv.push_back(nullptr);


      //forking it (to make the child run the process so that the program doesnt look hanged)
      pid_t pid = fork();

      if(pid < 0){
        perror("fork");
        continue;
      }
      
      if (pid == 0){
        //child run the programs if run nothing return but if it doesnt error then exit
        execvp(argv[0], argv.data());
        std::cerr << tokens[0] << ": not found" << std::endl;
        _exit(127);
      }else{
        //parent wait 
        int status;
        waitpid(pid, &status, 0);
      }
    }
  }

  
  
}
