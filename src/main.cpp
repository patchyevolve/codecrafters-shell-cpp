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
#include <array>
#include <readline/readline.h>
#include <readline/history.h>
#include <unordered_set>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <algorithm>


namespace fs = std::filesystem;

std::vector<std::string> history;
const size_t HISTORY_LIMIT = 1000;
std::string HISTFILE;
size_t session_start_index = 0;

enum class histpersistence { WRITE , APPEND };
constexpr histpersistence HIST_MODE = histpersistence::APPEND;

static std::vector<std::string> completion_pool;

static std::vector<std::string> path_exec_cache;
static std::string cached_path_env;

static bool path_cache_built = false;

//funtion to resolve histfile
std::string get_histfile(){

  const char* hf = getenv("HISTFILE");
  if(hf && *hf){
    return hf;
  }

  const char* home = getenv("HOME");
  if(home && *home){
    return std::string(home) + "/.my_shell_history";
  }

  return ".my_shell_history";
}

//read history on startup
size_t history_read_file(const std::string& path){

  std::ifstream in(path);
  if(!in.is_open()) return 0;

  size_t before = history.size();

  std::string line;
  while (std::getline(in, line)){
    if(line.empty()){
      continue;
    }
    history.push_back(line);
  }

  if(history.size() > HISTORY_LIMIT){
    size_t extra = history.size() - HISTORY_LIMIT;
    history.erase(history.begin() , history.begin() + extra);
  }

  size_t after = history.size();
  return (after > before) ? (after - before) : 0;

} 

//write history file (overwrite)
bool history_write_file(const std::string& path){
  std::ofstream out(path , std::ios::trunc);
  if(!out.is_open()) return false;

  for(auto& cmd : history){
    out << cmd << std::endl;
  }

  return true;
}

//appending new command on exit
bool history_append_file (const std::string& path , size_t from_index){
  if(from_index > history.size()){
    return true;
  }

  std::ofstream out(path, std::ios::app);
  if(!out.is_open()) return false;

  for (size_t i = from_index; i < history.size(); ++i){
    out << history[i] << std::endl;
  }
  return true;
}

std::vector<std::string> builtins = { "exit" , "echo" , "type", "pwd", "cd", "history"};

bool is_Builtin(std::string command){
  for(const auto& builtin : builtins){
    if(command == builtin){
      return true;
    }
  }
  return false;
}

static inline bool starts_with(const std::string&s ,const std::string& pref){
  return s.size() >= pref.size() && s.compare(0, pref.size(), pref ) == 0;
}

static bool only_spaces_befor_start(int start){
  for(int i =0 ; i < start; i++){
  if(!isspace((unsigned char)rl_line_buffer[i])) return false;
  }   
  return true;
}

static std::vector<std::string> split_path_env (const std::string& path_env){

  std::vector<std::string> dirs;

  size_t i =0;
  while( i <= path_env.size()){
    size_t j = path_env.find(':',i);
    std::string part = (j == std::string::npos) ? path_env.substr(i) : path_env.substr(i, j-i);
    if (part.empty()){
      part = ".";
    }
    dirs.push_back(part);
    if(j == std::string::npos) break;
    i = j+ 1;
  }
  return dirs;
}

static void rebuild_path_exec_cache(){
  const char* env = getenv("PATH");
  std::string cur = env ? std::string(env) : std::string();

  if(cur == cached_path_env && path_cache_built) return;

  cached_path_env = cur;
  path_exec_cache.clear();
  path_cache_built= true;

  std::unordered_set<std::string> seen;
  auto dirs = split_path_env(cur);

  for(const auto& dir : dirs){
    try {
      for(const auto& entry : fs::directory_iterator(dir)){
        if(!entry.is_regular_file()) continue;

        auto p = entry.path();
        std::string name = p.filename().string();
        if(!name.empty() && name[0] == '.') continue;
        std::string full = p.string();
        if(access(full.c_str() , X_OK) != 0) continue;
        
        if(seen.insert(name).second){
          path_exec_cache.push_back(name);
        }
      }
    }
    catch(...) {
      //unreadable dirs
    }
  }
}

//readline generator
static char* completion_generator(const char* text, int state){
  static size_t idx =0;
  if(state == 0) idx = 0;

  std::string pref = text? std::string(text) : std::string();

  while (idx < completion_pool.size()){
    const std::string& cand = completion_pool[idx++];
    if(starts_with(cand, pref)){
      return strdup(cand.c_str());
    }
  }
  return nullptr;
}



//readline completion callback
static char** shell_completion(const char* text , int start, int end){
  (void)end;


  if(start ==  0 || only_spaces_befor_start(start)){
    completion_pool.clear();

    std::unordered_set<std::string> seen;

    for(auto& b : builtins) {
      if(seen.insert(b).second) completion_pool.push_back(b);
    }

    rebuild_path_exec_cache();

    for(auto& e : path_exec_cache){ 
      if(seen.insert(e).second) completion_pool.push_back(e);
    }

    std::sort(completion_pool.begin(), completion_pool.end());
    return rl_completion_matches(text, completion_generator);
  }

  return rl_completion_matches(text, rl_filename_completion_function);
}

static void init_readline_completion(){
  rl_attempted_completion_function = shell_completion;
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

      if (!in_quotes && c == '|'){
        if(!current.empty()){
          tokens.push_back(current);
          current.clear();
        }  
        tokens.push_back("|");
        continue;
      }

      if (!in_quotes && (c == '>' || c == '<')){
        if(!current.empty()){
          tokens.push_back(current);
          current.clear();
        }
        
          
        if (c == '>' && i+1 < cmd.size() && cmd[i+ 1] == '>') {
          tokens.push_back(">>");
          i++;
        }else{
          tokens.push_back(std::string(1,c));
        }
        continue;
      }

      if(!in_quotes && (c == '1' || c == '2') && i+1 < cmd.size() && cmd[i+1] == '>'){
        if(!current.empty()){
          tokens.push_back(current);
          current.clear();
        }

        if(i+2 < cmd.size() && cmd[i+2] == '>'){
          tokens.push_back(std::string(1,c) + ">>");
          i += 2;
        }else{
          tokens.push_back(std::string(1,c) + ">");
          i+=1;
        }
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

bool RD_apply( const std::vector<Redirection>& redirs, bool in_child){

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
      if(in_child) _exit(1);
      return false;
    }

    if(dup2(fd, r.fd) < 0) {
        perror("dup2");
        if(in_child) _exit(1);
        return false;
    }
    close(fd);

  }
  return true;
}

struct command {
  std::vector<std::string> argv;
  std::vector<Redirection> redirs;
  bool is_builtin = false;
};

std::vector<std::vector<std::string>> split_by_pipe (const std::vector<std::string>& tokens){

  std::vector<std::vector<std::string>> parts;
  std::vector<std::string> cur;

  for (const auto& t : tokens){
    if(t == "|"){
      parts.push_back(cur);
      cur.clear();
    }
    else {
      cur.push_back(t);
    }
  }

  parts.push_back(cur);
  return parts;

}

std::vector<command> parse_pipeline (const std::vector<std::string>& tokens){
  auto parts = split_by_pipe(tokens);

  for(size_t i = 0; i < parts.size(); i++){
    if (parts[i].empty()) {
      throw std::runtime_error("syntax error near unexpected token `|`");
    }
  }

  std::vector<command> cmds;
  cmds.reserve(parts.size());

  for(auto & seg : parts){
    auto [argv_str, redirs] = RD_tokens(seg);
    if(argv_str.empty()){
      throw std::runtime_error("syntax error: empty command in pipeline");
    }

    command c;
    c.argv = std::move(argv_str);
    c.redirs = std::move(redirs);
    c.is_builtin = is_Builtin(c.argv[0]);
    cmds.push_back(std::move(c));
  }

  return cmds;
}

int run_builtin(const std::vector<std::string>& argv, bool in_child){

  (void)in_child;
  if(argv.empty()) return 0;
  const std::string & cmd = argv[0];

  if(cmd == "exit"){
    return 0;
  }

  else if(cmd == "pwd"){
    try{
      fs::path currentPath = fs::current_path();
      std::cout << currentPath.string() << std::endl;
    }
    catch (const fs::filesystem_error & e) {
      std::cerr << "filesystem error : " << e.what() << std::endl;
      return 1;
    }
    return 0;
  }

  else if (cmd == "echo") {
    for( size_t i = 1 ; i < argv.size() ; ++i){
            std::cout << argv[i];
            if(i + 1 < argv.size()){
              std::cout << " ";
            }
    }
    std::cout << std::endl;
    return 0;
  }

  else if(cmd == "cd"){

    //resolving the target
    std::string target;
    fs::path old_pwd = fs::current_path();


    if (argv.size() == 1){

      char* home = getenv("HOME");
      if(!home){
        std::cerr << "cd: HOME not set" << std::endl;
        return 1;
      }
      else { target = home; }

    }
    else if (argv[1] == "-"){

      char* old = getenv("OLDPWD");

      if(!old){
        std::cerr << "cd: OLDPWD not set" << std::endl;
        return 1;
      }
      else { target = old; }
            
    }
  else{
    target = argv[1];

    if(!target.empty() && target[0] == '~'){

      char* home = getenv("HOME");
                
      if(!home){
      std::cerr << "cd: HOME not set" << std::endl;
      }
      else {
        if(target.size() == 1 ){
        target = home;
        }
        else if( target[1] == '/'){
          target = std::string(home) + target.substr(1);
        }
      }
    }
  }

  if(target.empty()){
    return 1;
  }
  //changing the directory
  else if(chdir(target.c_str()) != 0){
    std::cerr << "cd: " << target << ": No such file or directory" << std::endl;
    return 1;
  }
  else{
    //update the enviorment
    fs::path new_pwd = fs::current_path();
    setenv("OLDPWD", old_pwd.string().c_str(), 1);
    setenv("PWD", new_pwd.string().c_str(), 1);
    }
  return 0;

  } 

  else if (cmd == "type"){
          
          //if no commad is provide after the type edge case
          if(argv.size() < 2){
            std::cout << "type: missing operand\n";
            return 1;
          }
          
            // if the command after type are builtin then just showing they are built in
            if(is_Builtin(argv[1])){
              std::cout<< argv[1] << " is a shell builtin" << std::endl;
              return 0;
            }

            
              
          //get path
          char* path_env = getenv("PATH");
          if(!path_env){
            std::cout << argv[1] << ": not found" << std::endl;
            return 1;
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

          for(const auto& dir : dirs){
          std::string fullPath = dir + "/" + argv[1];

            if(access(fullPath.c_str(), F_OK) != 0){
              continue;
            }

            if(access(fullPath.c_str(), X_OK) != 0){
              continue;
            }

            std::cout << argv[1] << " is " << fullPath << std::endl;    
            return 0;
          }

                
    std::cout << argv[1] << ": not found" << std::endl;
    return 1;
                
  }
     
  
  else if (cmd == "history"){

    if(argv.size() == 3 && argv[1] == "-a"){
      if(!history_append_file(argv[2], session_start_index)){
        std::cerr << "history: failed to append file" << std::endl;
        return 1;
      }
      session_start_index = history.size();
      return 0;
    }

    if(argv.size() == 3 && argv[1] == "-w"){
      if(!history_write_file(argv[2])){
        std::cerr << "history: failed to write file" << std::endl;
        return 1;
      }
      session_start_index = history.size();
      return 0;
    }

    if(argv.size() ==  3 && argv[1] == "-r"){
      size_t added = history_read_file(argv[2]);

      size_t start = (added <= history.size()) ? (history.size() - added) : 0;
      for (size_t i = start; i < history.size(); ++i){
        add_history(history[i].c_str());
      }

      session_start_index = history.size();
      return 0;
    }

    if(argv.size() == 2 && argv[1] == "-c"){
      history.clear();
      clear_history();
      std::ofstream(HISTFILE, std::ios::trunc).close();
      session_start_index = 0;
      return 0;
    }

    size_t start = 0;
    if(argv.size() == 2){
      try{
        int n = std::stoi(argv[1]);
        if(n < 0) n=0;
        if((size_t)n < history.size()) start = history.size() - (size_t)n;
      }catch(...){

      }
    }

    for (size_t i = start; i < history.size(); ++i){
      std::cout << (i+1) << " " << history[i] << std::endl;
    }
    return 0;
  }

  return 1;     

}

std::vector<char*> make_argv(const std::vector<std::string> & args){

  std::vector<char*> out;
  out.reserve(args.size() +  1);
  for(auto &s : args){
    out.push_back(const_cast<char*>(s.c_str()));
  }
  out.push_back(nullptr);
  return out;

}

int execute_pipeline(const std::vector<command>& cmds){
  
  int n = (int)cmds.size();
  if(n == 0){
    return 0;
  }

  std::vector<std::array<int, 2>> pipes;
  pipes.resize(std::max(0, n-1));


  for(int i = 0 ; i < n-1 ; i++){
    if(pipe(pipes[i].data()) < 0){
      perror("pipe");
      return 1;
    }
  }

  std::vector<pid_t> pids;
  pids.reserve(n);

  for(int i =0 ; i < n ; i++){
    
    pid_t pid = fork();

    if(pid < 0){
      perror("fork");
      return 1;
    }
  

    if(pid == 0){
      if(i > 0){
        if(dup2(pipes[i-1][0], 0) < 0){
          perror("dup2 stdin");
          _exit(1);
        }
      }
      
      if(i < n -1 ){
        if(dup2(pipes[i][1], 1) < 0){
          perror("dup2 stdout");
          _exit(1);
        }
      }

      for (int k =0 ; k < n-1 ; k++){
        close(pipes[k][0]);
        close(pipes[k][1]);
      }
    

      if(!RD_apply(cmds[i].redirs, true)) _exit(1);

      if(cmds[i].is_builtin){
          int st = run_builtin(cmds[i].argv, true);
          _exit(st);
      }
      else{
         auto argv = make_argv(cmds[i].argv);
          execvp(argv[0], argv.data());
          std::cerr << cmds[i].argv[0] << ": not found " << std::endl;
          _exit(127);
      }
    }
    
    pids.push_back(pid);

  }

  for(int i = 0 ; i < n-1 ; i++){
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  int last_status = 0;
  for(int i = 0 ; i < n ; i++){
    int st =0 ;
    waitpid(pids[i], &st, 0);
    if(i == n-1 ){
      last_status = st;
    }
  }

  if(WIFEXITED(last_status)){
    return WEXITSTATUS(last_status);
  }
  return 1;

}

bool expand_history(std::string& cmd, const std::vector<std::string>& history){
  if(cmd.size() < 2 || cmd[0] != '!') return true;

  if(cmd.find(' ') != std::string::npos) return true;

  try{
    if(cmd == "!!"){
      if(history.empty()){
        std::cerr << "history: event not found" << std::endl;
        return false;
      }
      cmd = history.back();
      
      return true;
    }

    if(cmd.size() >=  3 && cmd[1] == '-'){
      int n = std::stoi(cmd.substr(2));
      if(n <= 0 || (size_t)n > history.size()){
        std::cerr << "history: event not found" << std::endl;
        return false;
      }

      cmd = history[history.size() - n];
      
      return true;
    }


    int idx = std::stoi(cmd.substr(1));
    if(idx <= 0 || (size_t)idx > history.size()){
      std::cerr << "history: event not found" << std::endl;
      return false;
    }

    cmd = history[idx -1 ];
    
    return true;
  }
  catch(...){
    std::cerr << "history: event not found" << std::endl;
    return false;
  }

}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  HISTFILE = get_histfile();
  stifle_history((int)HISTORY_LIMIT);
  history_read_file(HISTFILE);
  session_start_index = history.size();
  for(auto& h : history) add_history(h.c_str());

  init_readline_completion();

  while(true){
    
    char* line = readline("$ ");
    if(!line){
      std::cout << std::endl;
      if constexpr (HIST_MODE == histpersistence::APPEND){
        history_append_file(HISTFILE, session_start_index);
      }
      else {
        history_write_file(HISTFILE);
      }
      break;
    }

    std::string cmd(line);
    free(line);

    auto is_blank = [](const std::string& s){
      for(char c : s) {
        if (!isspace((unsigned char)c)){
          return false;
        }
      }
      return true;
    };

    if(is_blank(cmd)){
      continue;
    }

    if(!expand_history(cmd , history)){
      continue;
    }


    std::vector<std::string> tokens;
    tokens = tokenizer(cmd);
    if(tokens.empty()) continue;

    bool store_in_history = true;
    if(tokens[0] == "history"){

      if(tokens.size() ==  2 && tokens[1] == "-c"){
        store_in_history = false;
      }
      else if(tokens.size() == 3 && tokens[1] == "-r" ){
        store_in_history = true;
      }
    }

    if(store_in_history){
      history.push_back(cmd);

      if(history.size() > HISTORY_LIMIT){
        size_t extra = history.size() - HISTORY_LIMIT;
        history.erase(history.begin() , history.begin() + extra);
      }

      add_history(cmd.c_str());
    }

    bool has_pipes = false;
    for(auto& t : tokens){
      if(t == "|"){
        has_pipes = true;
        break;
      }
    }

    // main command loop
    try{

      if(has_pipes){
        auto cmds = parse_pipeline(tokens);
        execute_pipeline(cmds);
        continue;
      }

      auto [argv_str, redirs] = RD_tokens(tokens);
      if(argv_str.empty()) continue;

      if (is_Builtin(argv_str[0])){
        FDSave saved = save_FD();
        if(!RD_apply(redirs,false)) {
          restorFD(saved);
          continue;
        }
         
        if(argv_str[0] ==  "exit"){
          restorFD(saved);
          if constexpr (HIST_MODE == histpersistence::APPEND){
            history_append_file(HISTFILE, session_start_index);
          }
          else {
            history_write_file(HISTFILE);
          }
          break;
        }

        run_builtin(argv_str, false);
        restorFD(saved);
        continue;

      }

      std::vector<char*> argv = make_argv(argv_str);

      pid_t pid = fork();
      if(pid < 0){
        perror("fork");
        continue;
      }

      if(pid == 0) {
        if(!RD_apply(redirs,true)){
          _exit(1);
        }
        execvp(argv[0], argv.data());
        std::cerr << argv_str[0] << ": not found" << std::endl;
        _exit(127);
      }
      else{
        int status;
        waitpid(pid, &status, 0);
      }

    }
    catch(const std::exception& e){
      std::cerr << e.what() << std::endl;
      continue;
    }
  }
}
