#include <iostream>
#include <vector>
#include <stdio.h>
#include "Random.h"
#include <fstream>
#include <climits>
#include <bitset>
#include <deque>

#define MAX_V_SPACE 64
using namespace std;

extern RandomGen* rnd;
//extern unsigned int process_cnt;
extern unsigned int frame_cnt;
extern unsigned long instr_cnt;


class PTE{
public:
    unsigned int valid :1;
    unsigned int write_protect:1;
    unsigned int referenced:1;
    unsigned int modified :1;
    unsigned int pageout:1;
    unsigned int filemapped:1;
    unsigned int frame:7;
    unsigned int in_vma:1;
    void reset();
    PTE():valid(0), modified(0), write_protect(0), referenced(0),pageout(0),filemapped(0),frame(0),in_vma(0){};
};


struct VMA{
    unsigned int start;
    unsigned int end;
    bool write_protect;
    bool filemapped;
};


class Process{
public:
    unsigned int id;
    PTE* page_table[64];
    vector<VMA*> vma_list;
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;
    bool is_vpage_filemapped(int vpage);
    bool is_vpage_protected(int vpage);
    void print_table();
    VMA* get_vpage(int vpage);
    Process();
};


class Frame{
public:
    PTE* pte;
    int id;
    bool is_free, is_mapped;
    unsigned int process_id, vpage;
    bitset<32> bitcount;
    unsigned int last_time;
    Frame():pte(nullptr),is_free(false),is_mapped(false),process_id(-1),vpage(-1),bitcount(0),last_time(0){};
    void free_frame();
};

class Pager{
public:
    vector<Frame*> queue;
    unsigned int hand;
    vector<Frame*> frame_table;
    Pager():hand(0){};
    virtual Frame* select_victim_frame() = 0;
    virtual void update(Frame *f);
};

class FIFO: public Pager{
public:
    FIFO() = default;
    Frame* select_victim_frame() override;
};

class Rand:public Pager{
public:
    Rand() = default;
    Frame* select_victim_frame() override;
};

class Clock:public Pager{
public:
    Clock() = default;
    Frame* select_victim_frame() override;
};

class NRU: public Pager{
public:
    unsigned int time;
    void reset_bit();
    NRU():time(0){};
    Frame* select_victim_frame() override ;
};

class Aging:public Pager{
public:
    Aging() = default;
    Frame* select_victim_frame() override;
};

class Working_Set: public Pager{
public:
    Working_Set()=default;
    Frame* select_victim_frame() override;
};