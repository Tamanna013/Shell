#include<iostream>
#include<string>
#include<unistd.h>
#include<filesystem>
#include<sstream>
#include<sys/wait.h>
#include<vector>
#include<fstream>
#include<string>
#include<fcntl.h>
#include<algorithm>
#include<cstdlib>

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
      if(input[5]=='\'' && input.find('>')==string::npos){
        int count=1;
        for(int i=6;i<input.length();i++){
          if(input[i]=='\'' && (++count)%2==0){
            while(i<input.length() && input[i]==' ') i++;
          }
          else if(input[i]=='\'' && count%2==1){
            
          }
          else if(input[i]=='\\' && input[i+1]=='\\'){
            i++;
            if(i<input.length()){
              cout<<input[i];
            }
          }
          else if(input[i]=='\\' && input[i+1]=='\"'){
            cout<<input[i];
            i++;
            cout<<input[i];
          }
          else cout<<input[i];
        }
        cout<<endl;
      }
      else if(input[5]=='\"' && input.find('>')==string::npos){
        int count=1;
        for(int i=6;i<input.length();){
          if(input[i]=='\"' && (++count)%2==0){
            i++;
            while(i<input.length() && input[i]==' ') i++;
            i--;
          }
          else if(input[i]=='\"' && count%2==1){
            i++;
          }
          else if(input[i]=='\\' && input[i+1]=='\\'){
            i++;
            if(i<input.length()){
              cout<<input[i];
            }
            i++;
          }
          else if(input[i]=='\\' && input[i+1]=='\''){
            cout<<input[i];
            i++;
            cout<<input[i];
          }
          else if(input[i]=='\\' && input[i+1]=='\"'){
            i++;
            cout<<input[i];
            i++;
          }
          else cout<<input[i++];
        }
        cout<<endl;
      }
      else if(input.find('>')!=string::npos){
        int pos=input.find('>');
        if(input[pos-1]=='1') input.erase(pos-1, 1);
        string filename=input.substr(pos+1);
        string text=input.substr(5, pos-6);
        ofstream file(filename);
        if(file.is_open()){
          file<<text;
          file.close();
        }
      }
      else{
        for(int i=5;i<input.length();){
          if(input[i]=='\\'){
            i++;
            if(i<input.length()){
              cout<<input[i];
            }
          }
          else if(input[i]==' '){
            cout<<' ';
            while(i<input.length() && input[i]==' ') i++;
            i--;
          }
          else cout<<input[i];
          i++;
        }
        cout<<endl;
      }
    }
    else if(input.substr(0, 4)=="cat "){
      if(input.find('>')!=string::npos){
        int pos=input.find('>');
        if(input[pos-1]=='1') input.erase(pos-1, 1);
        pos=pos-1;
        string filename="";
        vector<string> filenames;
        for(int i=4;i<pos;i++){
          if(input[i]==' '){
            if(!filename.empty()){
              filenames.push_back(filename);
            }
            filename="";
          }
          else filename+=input[i];
        }
        if(!filename.empty()){
          filenames.push_back(filename);
        }
        ofstream file(input.substr(pos+2));
        sort(filenames.begin(), filenames.end());
        for(string& fname: filenames){
          ifstream inFile(fname);
          if(inFile.is_open()){
            string line;
            bool firstLine=true;
            while(getline(inFile, line)){
              if(!firstLine) file<<endl;
              file<<line<<endl;
              firstLine=false;
            }
            inFile.close();
          }
          else cerr<<"cat: "<<fname<<": No such file or directory"<<endl;
        }
        file.close();
        continue;
      }
      string filename="";
      bool inSingleQuote = false;
      bool inDoubleQuote = false;
      string allContent="";
      for(int i=4;i<input.length();i++){
        if(input[i]=='\'' && !inDoubleQuote){
          inSingleQuote=!inSingleQuote;
        }
        else if(input[i]=='\"' && !inSingleQuote){
          inDoubleQuote=!inDoubleQuote;
        }
        else if(input[i]==' ' && !inSingleQuote && !inDoubleQuote){
          if(!filename.empty()){
            ifstream file(filename);
            if(file.is_open()){
              string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
              allContent+=content;
              file.close();
            }
            else cerr<<"cat: "<<filename<<": No such file or directory"<<endl;
          }
          filename="";
        }
        else if(input[i]=='\\' && !inSingleQuote){
          i++;
          if(i<input.length()){
            filename+=input[i];
          }
        }
        else{
          filename+=input[i];
        }
      }
      if(!filename.empty()){
        ifstream file(filename);
        if(file.is_open()){
          // This reads the entire file into a string at once.
          // istreambuf_iterator<char>(file) → start of the file
          // istreambuf_iterator<char>() → end of the file (empty = "end" marker)
          // So it's basically saying: "make a string from start to end of file"
          // Instead of reading line by line with getline, this just slurps everything in one go.
          string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
          allContent+=content;
          file.close();
        }
        else cerr<<"cat: "<<filename<<": No such file or directory"<<endl;
      }
      cout<<allContent;
      if(!allContent.empty() && allContent.back()!='\n'){
        cout<<endl;
      }
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
    else if(input=="ls"){
      // Loop through everything inside the present working directory
      // fs::current_path() gets the active folder path
      for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        // entry.path().filename() isolates just the file name from the full path
        std::cout << entry.path().filename().string() << std::endl;
      }
    }
    else if(input.substr(0, 5)=="ls -1"){
      string filename="";
      int i=5;
      while(input[i]!='>' && i<input.length()){
        if(input[i]!=' ') filename+=input[i++];
        else i++;
      }
      string text="";
      vector<string> files;
      
      // Collect all filenames
      for(const auto& entry : fs::directory_iterator(filename)){
        files.push_back(entry.path().filename().string());
      }
  
      // Sort alphabetically
      sort(files.begin(), files.end());
  
      // Build the text with newlines
      for(const auto& file : files){
        if(file!=files.back()) text += file + "\n";
        else text += file;
      }
      int pos=input.find('>');
      if(pos!=string::npos){
        string newfilename=input.substr(pos+2);
        ofstream newfile(newfilename);
        if(newfile.is_open()){
          newfile<<text;
          newfile.close();
        }
        else cerr<<"ls: "<<newfilename<<": No such file or directory"<<endl;
      }
      else{
        if(!text.empty()) text.pop_back();
        cout << text;
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
      else if(cmd=="cat"){
        cout<<"cat is /usr/bin/cat"<<endl;
      }
      else if(cmd=="cd"){
        cout<<"cd is a shell builtin"<<endl;
      }
      else if(cmd=="ls"){
        cout<<"ls is a shell builtin"<<endl;
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
    else if(input[0]=='\''){
      pid_t pid=fork();
      int pos=input.substr(1).find('\'')+1;
      string filename=input.substr(1, pos-1);
      string newfilename="";
      for(int i=0;i<filename.length();i++){
        if(filename[i]=='\\' && i+1<filename.length() && filename[i+1]=='\\'){
          newfilename+='\\';
          i++;
        }
        else newfilename+=filename[i];
      }
      filename=newfilename;
      ifstream file(filename);
      vector<string> args;
      vector<char*> argsStr;
      stringstream ss(input.substr(pos+1));
      string word;
      while(getline(ss, word, ' ')){
        int wordPos=word.find('\\');
        if(wordPos!=-1 && word[wordPos+1]=='\\'){
          string newWord="";
          for(int i=0;i<word.length();i++){
            if(word[i]=='\\' && i+1<word.length() && word[i+1]=='\\'){
              newWord+='\\';
              i++;
            }
            else newWord+=word[i];
          }
          word=newWord;
        }
        args.push_back(word);
      }
      for(size_t i=0;i<args.size();i++){
        argsStr.push_back(const_cast<char*>(args[i].c_str()));
      }
      argsStr.push_back(nullptr);
      if(pid==0){
        execvp(filename.c_str(), argsStr.data());
        cout<<filename<<": file not found"<<endl;
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
    else if(input[0]=='\"'){
      pid_t pid=fork();
      int pos=input.substr(1).find('\"')+1;
      string filename=input.substr(1, pos-1);
      string newfilename="";
      for(int i=0;i<filename.length();i++){
        if(filename[i]=='\\' && i+1<filename.length() && filename[i+1]=='\\'){
          newfilename+='\\';
          i++;
        }
        else newfilename+=filename[i];
      }
      filename=newfilename;
      ifstream file(filename);
      vector<string> args;
      vector<char*> argsStr;
      stringstream ss(input.substr(pos+1));
      string word;
      while(getline(ss, word, ' ')){
        int wordPos=word.find('\\');
        if(wordPos!=-1 && word[wordPos+1]=='\\'){
          string newWord="";
          for(int i=0;i<word.length();i++){
            if(word[i]=='\\' && i+1<word.length() && word[i+1]=='\\'){
              newWord+=word[i];
              i++;
            }
            else newWord+=word[i];
          }
          word=newWord;
        }
        args.push_back(word);
      }
      for(size_t i=0;i<args.size();i++){
        argsStr.push_back(const_cast<char*>(args[i].c_str()));
      }
      argsStr.push_back(nullptr);
      if(pid==0){
        execvp(filename.c_str(), argsStr.data());
        cout<<filename<<": file not found"<<endl;
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
