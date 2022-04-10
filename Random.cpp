#include "Random.h"

RandomGen::RandomGen(string &path) {
    ifstream infile(path);
    string line;
    getline(infile,line);
    int tot = stoi(line);
    for(int i=1 ;i<tot;i++){
        getline(infile, line);
        randomnumb.push_back(stoi(line));
    }
    size = randomnumb.size();
    offset = 0;
}

int RandomGen::random(int seed){
    offset = offset % size;
    int retval =  randomnumb[offset++] % seed;
    return retval;
}

