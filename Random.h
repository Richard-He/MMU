#ifndef RANDOM_H
#define RANDOM_H
#include <vector>
#include <string>
#include <fstream>

using namespace std;

class RandomGen{
    vector<int> randomnumb;
    int size, offset;
public:
    RandomGen(string & path);
    int random(int burst);
};
#endif