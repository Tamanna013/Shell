#include <iostream>
#include <string>
using namespace std;
int main() {
  // Flush after every std::cout / std:cerr
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
      else{
        cout<<cmd<<": not found"<<endl;
      }
    }
    else{
      cout<<input<<": command not found"<<endl;
    }
  }
}
