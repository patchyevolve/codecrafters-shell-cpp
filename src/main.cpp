#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>   // getenv
#include <unistd.h>  // access
#include <sys/wait.h>
#include <filesystem>
#include <fcntl.h>
#include <cstdio>

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

struct Redirection {
  int fd;
  std::string filename;
  enum { TRUNC, APPEND, READ} mode;
};


struct FDSave{
  int in;
  int out;
  int err;
};

FDSave save_FD() {
  return { dup(0), dup(1), dup(2)};
}

void restorFD(const FDSave& s){

  dup2(s.in, 0);
  dup2(s.out, 1);
  dup2(s.err, 2);

  close(s.in);
  close(s.out);
  close(s.err);

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

        //inside double quote
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

std::pair< std::vector<std::string>, std::vector<Redirection> > RD_tokens (const std::vector<std::string>& tokens){

  std::vector<std::string> argv_tokens;
  std::vector<Redirection> redirs;
  
  
  size_t i = 0;

  while (i < tokens.size()){

    const std::string& tok = tokens[i];

    if (tok == ">" || tok == "1>" || tok == ">>" || tok == "<" || tok == "2>" || tok == "2>>" || tok == "1>>"){

      if (i + 1 == tokens.size()){
        throw std::runtime_error("missing filename");
      }

      Redirection redir;

      if (tok.size() >= 2 && tok[0] == '2' && tok[1] == '>' ){
        redir.fd = 2;
      }
      else if( tok == "<"){
        redir.fd = 0;
      }
      else {
        redir.fd = 1;
      }


      if  (tok == ">" || tok == "1>" || tok == "2>"){
        redir.mode = Redirection::TRUNC;
      }
      else if (tok == ">>" || tok == "2>>" || tok == "1>>"){
        redir.mode = Redirection::APPEND;
      }
      else if (tok == "<"){
        redir.mode = Redirection::READ;
      }

      redir.filename = tokens[i+1];

      redirs.push_back(redir);

      i+= 2;
      continue;
    }
    else{
      argv_tokens.push_back(tok);
      ++i;
    }

  }

  return { argv_tokens, redirs};

}


void RD_apply( const std::vector<Redirection> redirs){

  for( const auto& r : redirs){

    int fd;

    if ( r.mode == Redirection::READ){
      fd = open(r.filename.c_str(), O_RDONLY);
    }
    else if(r.mode == Redirection::TRUNC){
      fd = open(r.filename.c_str() , O_WRONLY | O_CREAT | O_TRUNC, 0644); //permission is set to 0644 (read/write for owners  and read for others)
    }
    else{
      fd = open(r.filename.c_str() , O_WRONLY | O_CREAT | O_APPEND, 0644);
    }

    if(fd < 0){
      perror(r.filename.c_str());
      _exit(1);
    }

    dup2(fd, r.fd);
    if(dup2(fd, r.fd) < 0) {
        perror("dup2");
        _exit(1);
    }
    close(fd);

  }

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
    try{

      auto [argv_str, redirs] = RD_tokens(tokens);

      if( argv_str.empty()) {
        continue;
      }

      if (is_Builtin(argv_str[0])){

        // exit command high priority check
        if(argv_str[0] == "exit"){ 
          break;
        }

        FDSave saved = save_FD();

        RD_apply(redirs);

        if(argv_str[0] == "cd"){


          //resolving the target
          std::string target;
          fs::path old_pwd = fs::current_path();


          if (argv_str.size() == 1){

            char* home = getenv("HOME");
            if(!home){
              std::cerr << "cd: HOME not set" << std::endl;
              continue;
            }
            target = home;

          }
          else if (argv_str[1] == "-"){

            char* old = getenv("OLDPWD");

            if(!old){
              std::cerr << "cd: OLDPWD not set" << std::endl;
              continue;
            }
            target = old;
            
            std::cout << target << std::endl;
          }
          else{
            target = argv_str[1];

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

        else if(argv_str[0] == "pwd"){
          try {
            fs::path currentPath = fs::current_path();
            std::cout << currentPath.string() <<std::endl;
          }
          catch ( const fs::filesystem_error& e) {
            std::cerr << "filesystem error : " << e.what() << std::endl;
          }
        }

        // if builtin is echo used for printing the string in shell
        else if ( argv_str[0] == "echo"){
          for( size_t i = 1 ; i < argv_str.size() ; ++i){
            std::cout << argv_str[i];
            if(i + 1 < argv_str.size()){
              std::cout << " ";
            }
          }
          std::cout << std::endl;
        }

        // if builtin is type used for checking the info and path {if not builtin}
        else if (argv_str[0] == "type"){
          
          //if no commad is provide after the type edge case
          if(argv_str.size() < 2){
            std::cout << "type: missing operand\n";
            continue;
          }

          // if the command after type are builtin then just showing they are built in
          if(is_Builtin(argv_str[1])){
            std::cout<< argv_str[1] << " is a shell builtin" << std::endl;
          }

          else{
            
            //get path
            char* path_env = getenv("PATH");
            if(!path_env){
              std::cout << argv_str[1] << ": not found" << std::endl;
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
              std::string fullPath = dir + "/" + argv_str[1];

              if(access(fullPath.c_str(), F_OK) != 0){
                continue;
              }

              if(access(fullPath.c_str(), X_OK) != 0){
                continue;
              }

              std::cout << argv_str[1] << " is " << fullPath << std::endl;
              found = true;
              break;
            }

            if(!found){
              std::cout << argv_str[1] << ": not found" << std::endl;
            }
          }

          continue;

        }

        restorFD(saved);

      }

      //the case for executing the non built in commands for external programs
      else{
        
          //building args from tokens
          std::vector<char*> argv;
          for(const auto& s : argv_str){
            argv.push_back(const_cast<char*>(s.c_str()));
          }
          argv.push_back(nullptr);


          //forking it (to make the child run the process so that the program doesnt look hanged)
          pid_t pid = fork();

          if(pid < 0){
            perror("fork");
            continue;
          }
          
          if (pid == 0){

            //applyiing redirections
            RD_apply(redirs);
            
            //child run the programs if run nothing return but if it doesnt error then exit
            execvp(argv[0], argv.data());
            std::cerr << argv_str[0] << ": not found" << std::endl;
            _exit(127);
          }else{
            //parent wait 
            int status;
            waitpid(pid, &status, 0);
          }
      }
    }
    catch(const std::exception& e){
      std::cerr << e.what() << std::endl;
      continue;
    }
  }
}
