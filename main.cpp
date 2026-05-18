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
#include<termios.h> 

using namespace std;
namespace fs=filesystem;

struct termios orig; // we'll store the original terminal settings here so we can restore them when we're done

bool pastTab=false; // to track if the user presses tab key twice

bool isExecutable(const std::string& filepath) {
    //access() returns 0 on success (has permission), -1 on failure
    return access(filepath.c_str(), X_OK) == 0;
}

void enableRawMode(){
  // tcgetattr reads the current terminal settings into 'orig'
  // STDIN_FILENO = 0 = standard input (keyboard)
  tcgetattr(STDIN_FILENO, &orig);

  struct termios raw = orig; // copy original settings so we can modify them

  // c_lflag = "local flags" — controls how the terminal processes input
  // ICANON = canonical mode (line buffering) — normally terminal waits
  //          for Enter before sending input to your program. We turn this OFF
  //          so every keypress is sent immediately.
  // ECHO   = automatically prints what the user types. We turn this OFF
  //          so we can control what gets printed ourselves.
  // ~(ECHO | ICANON) means: turn OFF both ECHO and ICANON bits
  raw.c_lflag &= ~(ECHO | ICANON);

  // c_cc = control characters array
  // VMIN  = minimum number of bytes read() should wait for before returning
  //         setting it to 1 means: return as soon as 1 character is available
  raw.c_cc[VMIN] = 1;

  // VTIME = timeout for read() in tenths of a second
  //         setting it to 0 means: no timeout, wait indefinitely
  raw.c_cc[VTIME] = 0;

  // tcsetattr applies our new settings
  // TCSAFLUSH = apply after flushing (clearing) any pending input
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode(){
  // restore the original terminal settings
  // IMPORTANT: always call this before your program exits!
  // if you don't, the terminal stays in raw mode and looks broken
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

vector<string> matches;

string checkIfTabComplete(string input){
  vector<string> commands={"echo", "cat", "pwd", "cd", "ls", "type", "exit"};
  char* currPATH=getenv("PATH");
  if(currPATH){
    string path_str(currPATH); //path_str converts the raw char* into a proper C++ string so we can work with it easily
    string dir;
    matches.clear();
    stringstream ss(path_str);
    // getline(ss, dir, ':') reads from ss until it hits a : character, puts that chunk into dir, and returns true if it successfully read something. So if PATH is /usr/bin:/bin:/usr/local/bin, the three iterations give:
    // dir = "/usr/bin"
    // dir = "/bin"
    // dir = "/usr/local/bin"
    while(getline(ss, dir, ':')){
      for(const auto& entry:fs::directory_iterator(dir)){
        string path=entry.path().filename().string();
        if(path.substr(0, input.length())==input && isExecutable(dir+"/"+path)) matches.push_back(path);
      }
    }
  }
  for(int i=0;i<commands.size();i++){
    if(commands[i].substr(0, input.length())==input){
      matches.push_back(commands[i]);
    }
  }
  sort(matches.begin(), matches.end());
  matches.erase(unique(matches.begin(), matches.end()), matches.end());
  if(matches.size()==1){
    return matches[0];
  }
  else{
    cout<<'\a'<<endl;
  }
  return input;
}

void handleTwoTabs(const string& input){
  for(auto match:matches){
    cout<<match<<" ";
  }
  cout<<"\n$ "<<input;
  matches.clear();
  cout<<endl;
}

string readInput(){
  string input="";
  char c;
  while(true){
    read(STDIN_FILENO, &c, 1); // read 1 byte from standard input into variable c, in raw mode this returns immediately when a key is pressed
    if(c=='\n'){
      cout<<endl;
      break;
    }
    // ── BACKSPACE ──
    // backspace sends ASCII 127 (DEL) on most terminals
    // some older terminals send ASCII 8 (\b) instead
    else if(c == 127 || c == 8){
      if(!input.empty()){
        input.pop_back(); // remove last character from our string

        // "\b \b" is the trick to visually erase a character:
        // \b = move cursor left one space
        // ' ' = print a space (overwrites the character)
        // \b = move cursor left again (so cursor is in right place)
        cout << "\b \b";
        cout.flush(); // make sure it shows immediately
      }
    }

    // ── ARROW KEYS & SPECIAL KEYS ──
    // special keys send ESCAPE SEQUENCES: 3 bytes starting with \x1b (ESC)
    // format: ESC [ X  where X is a letter
    // left arrow  = ESC [ D
    // right arrow = ESC [ C
    // up arrow    = ESC [ A  (used for command history)
    // down arrow  = ESC [ B  (used for command history)
    else if(c == '\x1b'){
      char seq[2];
      read(STDIN_FILENO, &seq[0], 1); // read '[' 
      read(STDIN_FILENO, &seq[1], 1); // read 'A', 'B', 'C', or 'D'

      if(seq[0] == '['){
        if(seq[1] == 'A'){
          // UP arrow — for command history (not implemented here)
          // you would load the previous command into input
        }
        else if(seq[1] == 'B'){
          // DOWN arrow — for command history (not implemented here)
        }
        else if(seq[1] == 'C'){
          // RIGHT arrow — move cursor right
          // (would need to track cursor position to implement properly)
        }
        else if(seq[1] == 'D'){
          // LEFT arrow — move cursor left
        }
      }
    }

    // ── CTRL+C ──
    // sends ASCII 3 (ETX = end of text)
    else if(c == 3){
      cout << "^C\n";
      input = ""; // clear input
      break;
    }

    // ── CTRL+D ──
    // sends ASCII 4 (EOT = end of transmission)
    // in a shell this usually means "exit"
    else if(c == 4){
      cout << '\n';
      disableRawMode();
      exit(0);
    }

    // ── TAB KEY ──
    else if(c=='\t' && pastTab){
      handleTwoTabs(input);
      cout<<"$ "<<input;
      pastTab=false;
    }
    else if(c=='\t'){
      string newInput=checkIfTabComplete(input);
      if(input!=newInput){
        // erase current input
        for(int i=0;i<input.length();i++){
          cout << "\b \b";
        }
        cout<<newInput<<' ';
        cout.flush();
        input=newInput+' ';
        pastTab=true;
      }
      else{
        pastTab=true;
        // no match or multiple matches - ring bell
        cout<<'\a'; // \a is the ASCII Bell character, which makes the terminal beep
        cout.flush();
      }
    }

    else{
      input+=c;
      cout<<c;
      cout.flush();
    }
  }
  return input;
}

int main() {
  //flush after every std::cout/std:cerr
  cout<<unitbuf;
  cerr<<unitbuf;

  enableRawMode();

  while(true){
    cout<<"$ ";
    string input=readInput();
    if(input=="exit") break;
    if(input.empty()) continue;
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
        if(input[pos-1]=='1' && input[pos+1]=='>'){
          string text=input.substr(5, pos-6);
          while(!text.empty() && text.back()==' ') text.pop_back();
          string filename=input.substr(pos+3);
          while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
          ofstream outFile(filename, ios::app);
          if(outFile.is_open()){
            outFile<<text<<"\n";
            outFile.close();
          }
          continue;
        }
        else if(input[pos-1]=='1') input.erase(pos-1, 1);
        else if(input[pos-1]=='2' && input[pos+1]=='>'){
          string text=input.substr(5, pos-6);
          while(!text.empty() && text.back()==' ') text.pop_back();
          string filename=input.substr(pos+3);
          while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
          ofstream errFile(filename, ios::app); 
          errFile.close();
          cout<<text<<endl;
          continue;
        }
        else if(input[pos-1]=='2'){
          input.erase(pos-1, 1);
          pos=pos-1;
          string text=input.substr(5, pos-5);
          string filename=input.substr(pos+2);
          while(!text.empty() && text.back()==' ') text.pop_back();
          while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
          ofstream errFile(filename); //create empty stderr file
          errFile.close();
          cout<<text<<endl;
          continue;
        }
        else if(input[pos+1]=='>'){
          string text=input.substr(5, pos-6);
          string filename=input.substr(pos+3);
          ofstream outFile(filename, ios::app);
          if(outFile.is_open()){
            outFile<<text;
            outFile.close();
          }
          else cerr<<"echo: "<<filename<<": No such file or directory"<<endl;
          continue;
        }
        string filename=input.substr(pos+1);
        while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
        string text=input.substr(5, pos-6);
        while(!text.empty() && text.back()==' ') text.pop_back();
        ofstream file(filename);
        if(file.is_open()){
          file<<text<<endl;
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
        else if(input[pos-1]=='2' && input[pos+1]=='>'){
        string errFilename=input.substr(pos+3);  // output file
        while(!errFilename.empty() && errFilename.front()==' ') errFilename=errFilename.substr(1);
        string fname="";
        vector<string> filenames;
        for(int i=4;i<pos-1;i++){
          if(input[i]==' '){
            if(!fname.empty()) filenames.push_back(fname);
              fname="";
            }
            else fname+=input[i];
          }
          if(!fname.empty()) filenames.push_back(fname);
          ofstream errFile(errFilename, ios::app);
          for(string& f: filenames){
            ifstream inFile(f);
            if(inFile.is_open()){
              string content((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
              cout<<content;
              inFile.close();
            }
            else errFile<<"cat: "<<f<<": No such file or directory"<<endl;
          }
          errFile.close();
          continue;
        }
        else if(input[pos-1]=='2'){
          string filename="";
          vector<string> filenames;
          for(int i=4;i<pos-1;i++){
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
          ofstream errFile(input.substr(pos+2), ios::app); //ios::app means "append mode", so instead of overwriting the file, it will add new content to the end of the file. This is important for stderr because you typically want to keep all error messages rather than just the last one.
          sort(filenames.begin(), filenames.end());
          for(string& fname: filenames){
            ifstream inFile(fname);
            if(inFile.is_open()){
              string content((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
              cout<<content;
              inFile.close();
            }
            else{
              errFile<<"cat: "<<fname<<": No such file or directory"<<endl;
            }
          }
          errFile.close();
          continue;
        }
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
      int pos=input.find('>');
      if(input[pos-1]=='2' && input[pos+1]=='>'){
        int pos=input.find('>');
        string filename=input.substr(pos+3);
        while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
        string dirname=input.substr(6, pos-7);
        while(!dirname.empty() && dirname.back()==' ') dirname.pop_back();
        ofstream errFile(filename, ios::app); //always create the output file first
        if(!fs::exists(dirname)){
          errFile<<"ls: "<<dirname<<": No such file or directory"<<endl;
          errFile.close();
          continue;
        }
        vector<string> files;
        for(const auto& entry : fs::directory_iterator(dirname)){
          files.push_back(entry.path().filename().string());
        }
        sort(files.begin(), files.end());
        for(const auto& file: files){
          errFile<<file<<endl;
        }
        errFile.close();
        continue;
      }
      else if(input[pos-1]=='2'){
        input.erase(pos-1, 1);
        pos=pos-1;
        string filename=input.substr(pos+2);
        ofstream errFile(filename);
        errFile<<"ls: "<<input.substr(6, pos-7)<<": No such file or directory"<<endl;
        errFile.close();
        continue;
      }
      else if(input[pos+1]=='>'){
        string filename=input.substr(pos+3);
        while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
        string dirname=input.substr(6, pos-7);
        while(!dirname.empty() && dirname.back()==' ') dirname.pop_back();
        ofstream outFile(filename, ios::app);
        if(!fs::exists(dirname)){
          cerr<<"ls: "<<dirname<<": No such file or directory"<<endl;
          outFile.close();
          continue;
        }
        vector<string> files;
        for(const auto& entry : fs::directory_iterator(dirname)){
          files.push_back(entry.path().filename().string());
        }
        sort(files.begin(), files.end());
        for(const auto& file: files){
          outFile<<file<<endl;
        }
        outFile.close();
        continue;
      }
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
    else if(input.substr(0, 3)=="ls "){
      int pos=input.find('>');
      if(pos!=string::npos){
        if(input[pos-1]=='1') input.erase(pos-1, 1);
        else if(input[pos-1]=='2' && input[pos+1]=='>'){
          string dirname=input.substr(3, pos-4);
          while(!dirname.empty() && dirname.back()==' ') dirname.pop_back();
          string filename=input.substr(pos+3);
          while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
          ofstream errFile(filename, ios::app);
          if(!fs::exists(dirname)){
            errFile<<"ls: "<<dirname<<": No such file or directory"<<endl;
            errFile.close();
            continue;
          }
          for(const auto& entry : fs::directory_iterator(dirname)){
            cout<<entry.path().filename().string()<<"\n";
          }
          errFile.close();
          continue;
        }
        else if(input[pos+1]=='>'){
          string dirname=input.substr(3, pos-4);
          while(!dirname.empty() && dirname.back()==' ') dirname.pop_back();
          string filename=input.substr(pos+3);
          while(!filename.empty() && filename.front()==' ') filename=filename.substr(1);
          vector<string> files;
          try{
            for (const auto& entry : fs::directory_iterator(dirname)) {
              files.push_back(entry.path().filename().string());
            }
            sort(files.begin(), files.end());
            ofstream outFile(filename, ios::app);
            for(const auto& file: files){
              outFile<<file<<endl;
            }
            outFile.close();
          }
          catch(const fs::filesystem_error& e){
            cerr<<"ls: "<<dirname<<": No such file or directory"<<endl;
          }
        }
      }
      string filename=input.substr(3);
      if(filename=="~") filename=getenv("HOME");
      try{
        for (const auto& entry : fs::directory_iterator(filename)) {
          std::cout << entry.path().filename().string() << std::endl;
        }
      }
      catch(const fs::filesystem_error& e){
        cerr<<"ls: "<<filename<<": No such file or directory"<<endl;
      }
      continue;
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
  disableRawMode(); // ALWAYS restore terminal before exiting!
  return 0;
}
