#include<iostream>
#include<string>
#include<unistd.h>
#include<filesystem>
#include<sstream>
#include<sys/wait.h>
#include<vector>
#include <fstream>
#include <string>

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
      if(input[5]=='\''){
        int count=1;
        for(int i=6;i<input.length();i++){
          if(input[i]=='\'' && (++count)%2==0){
            while(i<input.length() && input[i]==' ') i++;
          }
          else if(input[i]=='\'' && count%2==1){
            //do nothing, just skip the quote
          }
          else cout<<input[i];
        }
        cout<<endl;
      }
      else if(input[5]=='\"'){
        int count=1;
        for(int i=6;i<input.length();){
          if(input[i]=='\"' && (++count)%2==0){
            i++;
            while(i<input.length() && input[i]==' ') i++;
            i--;
          }
          else if(input[i]=='\"' && count%2==1){
            i++;
            //do nothing, just skip the double quote
          }
          else{
            cout<<input[i];
            i++;
          }
        }
        cout<<endl;
      }
      else{
        for(int i=5;i<input.length();i++){
          if(input[i]==' '){
            cout<<' ';
            while(i<input.length() && input[i]==' ') i++;
            i--;
          }
          else cout<<input[i];
        }
        cout<<endl;
      }
    }
    else if(input.substr(0, 4)=="cat "){
      string filename="";
      for(int i=5;i<input.length();i++){
        if(input[i]=='\''){
          if(!filename.empty()){
            ifstream file(filename);
            if(file.is_open()){
              string line;
              while(getline(file, line)){
                cout<<line;
              }
              file.close();
            }
            else cerr<<"cat: "<<filename<<": No such file or directory"<<endl;
          }
          filename="";
          i++;
          while(i<input.length() && input[i]!='\'') i++;
        }
        else if(input[i]=='\"'){

        }
        else{
          filename+=input[i];
        }
      }
      if(!filename.empty()){
        ifstream file(filename);
        if(file.is_open()){
          string line;
          while(getline(file, line)){
            cout<<line;
          }
          file.close();
        }
        else cerr<<"cat: "<<filename<<": No such file or directory"<<endl;
      }
      cout<<endl;
    }
    else if(input=="pwd"){
      cout<<fs::current_path().string()<<endl;
    }
    else if(input.substr(0, 3)=="cd "){
      fs::path PATH=input.substr(3);
      if(PATH=="~") PATH=fs::path(getenv("HOME"));
      try{
        fs::current_path(PATH);
      }
      catch(const fs::filesystem_error& e){
        cerr<<input.substr(0, 2)<<": "<<input.substr(3)<<": No such file or directory"<<endl;
      }
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
      else if(cmd=="pwd"){
        cout<<"pwd is a shell builtin"<<endl;
      }
      else if(cmd=="cd"){
        cout<<"cd is a shell builtin"<<endl;
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
      pid_t pid=fork();
      // Create a vector of char* to hold all arguments
      vector<string> args;
      vector<char*> argsStr; 
      int pos=input.find(' ');
      if(pos==-1){
        // No arguments, just the executable name
        argsStr.push_back(const_cast<char*>(input.c_str()));
        argsStr.push_back(nullptr); // execvp expects a NULL-terminated array
        if(pid==0){
          execvp(input.c_str(), argsStr.data());
          cout<<input<<": command not found"<<endl;
          exit(1);
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
        string exeName=input.substr(0, pos);
        stringstream ss(input.substr(pos+1));
        string word;

        //argv[0] must be the executable name
        //const_cast is used to convert const char* to char* because execvp expects char* array
        //exeName.c_str() gives a const char* pointer to the string data of exeName
        //exeName is the name of the executable, which is typically the first argument in the command line 
        args.push_back(exeName);

        while(getline(ss, word, ' ')){
          args.push_back(word);
        }

        for(size_t i=0;i<args.size();i++){
          argsStr.push_back(const_cast<char*>(args[i].c_str()));
        }

        // Crucial: The array must end with a NULL pointer
        argsStr.push_back(nullptr);
      
        if(pid==0){
          // Child Process: Execute the command
          //execvp will automatically scan your PATH directories to find exeName
          execvp(exeName.c_str(), argsStr.data());
          cout<<input<<": command not found"<<endl;
          exit(1);
        }
        else if(pid>0){
          int status;
          waitpid(pid, &status, 0);
        }
        else{
          cerr<<"Failed to fork"<<endl;
        }
      }
    }
  }
}
