#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <stdlib.h>
#include <iomanip>

using namespace std;

void split(string s, vector<string> &sv){
  stringstream ss(s);
  string token;
  while(getline(ss, token, '|')){
    sv.push_back(token);
  }
}

int get_type(string s){
  if(s.compare("EVENT_PARCEL_CREATE") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_SEND") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_RECV") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_RUN") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_END") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_SUSPEND") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_RESUME") == 0)
    return -2;
  if(s.compare("EVENT_PARCEL_RESEND") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PROBE_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PROBE_END") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PROGRESS_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PROGRESS_END") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PWC_SEND") == 0)
    return -2;
  if(s.compare("EVENT_NETWORK_PWC_RECV") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_END") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_MAIL_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_MAIL_END") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_POP_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_POP_END") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_EPOCH_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_EPOCH_END") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_NETWORK_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_NETWORK_END") == 0)
    return -2;
  if(s.compare("EVENT_SCHED_STEAL_BEGIN") == 0)
    return 0;
  if(s.compare("EVENT_SCHED_STEAL_END") == 0)
    return -2;
  if(s.compare("EVENT_TRACE_FILE_IO_BEGIN") == 0)
    return -2;
  if(s.compare("EVENT_TRACE_FILE_IO_END") == 0)
    return -2;
  return -2;
}

bool firstcomp(pair<long, int> a, pair<long, int> b){
  return a.first < b.first;
}

int main(int argc, char* argv[]){
  if(argc < 4) {
    cout<<"need input and output filenames and worker thread"<<endl;
    return 0;
  }

  string inputFile = argv[1];
  char *outputFile = argv[2];
  int worker = atoi(argv[3]);
  
  ofstream outfile;
  ifstream infile;
  string line;
  string sample;
  vector<pair<long, int> > timestamps;
  vector<string> pieces;

  int curtype = -2;
  stringstream u;
  u << argv[3];
  inputFile.append(u.str());
  inputFile.append(".txt");
  infile.open(inputFile.c_str());

  while(getline(infile, line)){
    sample = line.substr(0, 5);
    if(sample.compare("EVENT") == 0){
      curtype = get_type(line);
    }
    else{
      if (curtype == -2) {
        continue;
      }
      split(line, pieces);
      int which = atoi(pieces[0].c_str());
      long time = atol(pieces[1].c_str());
      if (which != worker){
        pieces.clear();
        continue;
      }
      if(time != 0){
        timestamps.push_back(make_pair(time, curtype));
      }
      pieces.clear();
    }
  }
  infile.close();

  sort(timestamps.begin(), timestamps.end(), firstcomp);
  
  long offset = 0;
  
  /*
  if(timestamps.size() > 1) {
    offset = timestamps[0].first;
  }
  for(int i = 0; i < timestamps.size(); i++){
    timestamps[i].first -= offset;
  }
  */

  string output = outputFile;
  stringstream o;
  o << worker;
  output.append(".");
  output.append(o.str());
  output.append(".txt");
  outfile.open(output.c_str());

  int count = 0;
  long interval = 0;
  long freq = 1000000;
  if (argc > 4) freq *= atoi(argv[4]);
  for(int i = 0; i < timestamps.size(); i++){
    if(timestamps[i].first > interval){
      outfile<<setprecision(9)<<interval/1e9<<" "<<count<<"\n";
      interval += freq;
      count = 0;
    }
    else count++;
  }
  outfile<<setprecision(9)<<interval/1e9<<" "<<count<<"\n";
  outfile.close();
} 

