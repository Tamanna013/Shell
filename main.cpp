#include<iostream>
#include<string>
#include<unistd.h>
#include<filesystem>
#include<sstream>
#include<sys/wait.h>

using namespace std;
namespace fs=filesystem;

bool isExecutable(const std::string& filepath) {
    //access() returns 0 on success (has permission), -1 on failure
    return access(filepath.c_str(), X_OK) == 0;
}
int main() {
  //flush after every std::cout/std:cerr
  cout<<unitbuf;
  cerr<<unitbuf;

  while(true){
    cout<<"$ ";
    string input;
    getline(cin, input);
    if(input=="exit") break;
    if(input.substr(0, 5)=="echo "){
      cout<<input.substr(5)<<endl;
    }
    else if(input.substr(0, 5)=="type "){
      string cmd=input.substr(5);
      if(cmd=="echo"){
        cout<<"echo is a shell builtin"<<endl;
      }
      else if(cmd=="type"){
        cout<<"type is a shell builtin"<<endl;
      }
      else if(cmd=="exit"){
        cout<<"exit is a shell builtin"<<endl;
      }
      else if(filesystem::exists("/bin/"+cmd) && !isExecutable("/bin/"+cmd)){
        //skip non-executable files
      }
      else{
        char* path_env=std::getenv("PATH");
        bool found=false;
        if(path_env){
          string path_str(path_env), dir;
          stringstream ss(path_str); //it splits path by ':'
          while(getline(ss, dir, ':')){
            if(filesystem::exists(dir+"/"+cmd) && isExecutable(dir+"/"+cmd)){
              cout<<cmd<<" is "<<filesystem::canonical(dir+"/"+cmd).string()<<endl;
              found=true;
              break;
            }
          }
        }
        if(!found){
          cout<<cmd<<": not found"<<endl;
        }
      }
    }
    else if(isExecutable(input)){
      // Create a vector of char* to hold all arguments
      std::vector<char*> args;
      int pos=input.find(' ');
      stringstream ss(input.substr(pos+1));
      string word;
      while(getline(ss, word, ' ')){
        arg_vec.push_back(word);
      }

      //argv[0] must be the executable name
      //const_cast is used to convert const char* to char* because execvp expects char* array
      //exeName.c_str() gives a const char* pointer to the string data of exeName
      //exeName is the name of the executable, which is typically the first argument in the command line 
      args.push_back(const_cast<char*>(exeName.c_str()));

      //argv[1] is your standalone arg1 string
      args.push_back(const_cast<char*>(arg1.c_str()));

      //argv[2] onwards are the elements from your array
      for (const auto& arg : restArgs) {
        args.push_back(const_cast<char*>(arg.c_str()));
      }

      // Crucial: The array must end with a NULL pointer
      args.push_back(nullptr);
      
      pid_t pid=fork();
      if(pid==0){
        // Child Process: Execute the command
        //execvp will automatically scan your PATH directories to find exeName
        execvp(exeName.c_str(), args.data());
      }
      else if(pid>0){
        int status;
        waitpid(pid, &status, 0);
      }
      else{
        cerr<<"Failed to fork"<<endl;
      }
    }
    else{
      cout<<input<<": command not found"<<endl;
    }
  }
}
