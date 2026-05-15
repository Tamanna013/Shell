#include<iostream>
#include<string>
#include<unistd.h>
#include<filesystem>
#include<sstream>
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
    else{
      cout<<input<<": command not found"<<endl;
    }
  }
}
