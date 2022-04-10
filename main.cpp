#include <iostream>
#include <vector>
#include <cstdio>
#include <sstream>
#include <bitset>
#include <unistd.h>
#include <cstring>
#include <climits>
#include "Random.h"
#include "MM.h"
#include "Instruction.h"
#define DEBUG
using namespace std;

unsigned int process_cnt=0 ;
unsigned int frame_cnt = 0;
unsigned long instr_cnt = 0;
bool O=false, P=false, F=false, S=false;
vector<Instruction*> instr_list;
vector<Frame*>free_frame_list;
vector<Frame*>frame_table;
vector<Process*>process_list;
RandomGen* rnd;
Pager* pager;
Frame* get_frame(){
    Frame* frame;
    if(free_frame_list.empty()){
        frame = pager->select_victim_frame();
        PTE* pre_pte = frame->pte;
        pre_pte->valid = false;

        Process * process = process_list[frame->process_id];
        if(O){
            printf(" UNMAP %d:%d\n",frame->process_id,frame->vpage);
        }
        process->unmaps++;

        if(pre_pte->modified){
            if(pre_pte->filemapped){
                if(O)
                    printf(" FOUT\n");
                process->fouts++;
            }
            else{
                if(O)
                    printf(" OUT\n");
                pre_pte->pageout=true;
                process->outs++;
            }
            pre_pte->referenced =false;
            pre_pte->modified=false;
        }
    }
    else{
        frame = free_frame_list.front();
        frame->is_free =false;
        free_frame_list.erase(free_frame_list.begin());
        return frame;
    }
    return frame;
}

Instruction* get_next_instruction() {
    if (instr_list.empty()) {
        return nullptr;
    }
    Instruction* instr = instr_list.front();
    instr_list.erase(instr_list.begin());
    return instr;
}
bool get_next_line(ifstream &file, string& linebuf){
    if(file.eof()){
        return false;
    }
    getline(file, linebuf);
    while(!file.eof()){
        if(linebuf[0]=='#'){
            getline(file, linebuf);
            continue;
        }
        else return true;
    }
}
void read_file(string& filename){
    ifstream file(filename);
    stringstream ss;
    string line;
    get_next_line(file,line);
    process_cnt = stoi(line);
    int vma_cnt;
    for(int i =0;i<process_cnt;i++){
        get_next_line(file,line);
        vma_cnt = stoi(line);
        auto *p = new Process();
        p->id = i;
        process_list.push_back(p);
        for (int j=0;j<vma_cnt;j++){
            VMA* vma = new VMA();
            get_next_line(file, line);
            ss.str("");
            ss.clear();
            ss<<line;
            ss>>vma->start>>vma->end>>vma->write_protect>>vma->filemapped;
            p->vma_list.push_back(vma);
        }
    }
    while(get_next_line(file, line)){
        char c;
        int n;
        ss.str("");
        ss.clear();
        ss<<line;
        ss>>c>>n;
        if(c==' ')break;
        auto *instr =new Instruction ;
        instr->op =c;
        instr->vpage =n;
        instr_list.push_back(instr);
    }
}

void create_frame_table(){
    Frame *frame;
    for( int i=0; i<frame_cnt; i++){
        frame = new Frame();
        frame->id= i;
        frame->is_free = true;
        frame_table.push_back(frame);
        free_frame_list.push_back(frame);
    }
}

void create_pager(char opt){
    switch (opt){
        case 'f': pager = new FIFO();break;
        case 'r': pager = new Rand();break;
        case 'c': pager = new Clock();break;
        case 'e': pager = new NRU();break;
        case 'a': pager = new Aging();break;
        case 'w': pager = new Working_Set();break;
        default: break;
    }
    pager->frame_table = frame_table;
}

int main(int argc, char* argv[]){
    string infile,rfile;
    char c;
    char*algos, *options;
    //Parse Args
    while((c=getopt(argc,argv,"a:f:o:"))!=-1){
        switch (c)
        {
            case 'a':
                algos = optarg;
                break;
            case 'o':
                for(int i =0;i<strlen(optarg);i++){
                    if(optarg[i] == 'O')
                        O=true;
                    else if(optarg[i]=='P')
                        P=true;
                    else if (optarg[i]=='F')
                        F=true;
                    else if (optarg[i]=='S')
                        S=true;
                }
                break;
            case 'f':
                frame_cnt = stoi(optarg);
                break;
            default:
                break;
        }
    }
    infile = argv[optind++];
    rfile = argv[optind];
    read_file(infile);
    rnd = new RandomGen(rfile);

    //Create Frame Table
    create_frame_table();
    //Create Pager
    create_pager(algos[0]);

    //Begin
    Process *cur_proc;
    unsigned long context_switchs = 0,proc_exits = 0;
    unsigned long long cost = 0;
    unsigned int accesses =0;
    Instruction* instr;
    Frame* frame;
    while((instr = get_next_instruction())!= nullptr) {
        char op = instr->op;
        int vpage = instr->vpage;
        if (O) {
            printf("%lu: ==> %c %d\n", instr_cnt, op, vpage);
        }
        instr_cnt++;

        if (op == 'c') {
            cur_proc = process_list[vpage];
            context_switchs++;
            continue;
        } else if (op == 'e') {
            printf("EXIT current process %d\n", vpage);
            proc_exits++;
            PTE *pte;
            for (int i = 0; i < MAX_V_SPACE; i++) {
                pte = cur_proc->page_table[i];
                if (pte->valid) {
                    if (O)
                        printf(" UNMAP %d:%d\n", cur_proc->id, i);
                    cur_proc->unmaps++;
                    Frame *f = frame_table[pte->frame];
                    f->free_frame();
                    free_frame_list.push_back(f);
                }
                if (pte->modified && pte->filemapped) {
                    if (O)
                        printf(" FOUT\n");
                    cur_proc->fouts++;
                }
                pte->reset();
            }
            cur_proc = nullptr;
            continue;
        }
        accesses++;
        PTE *pte = cur_proc->page_table[vpage];
        if (!pte->in_vma) {
            if (cur_proc->get_vpage(vpage)) {
                if (cur_proc->is_vpage_filemapped(vpage))
                    pte->filemapped = true;
                if (cur_proc->is_vpage_protected(vpage))
                    pte->write_protect = true;
                pte->in_vma = true;
            } else {
                printf(" SEGV\n");
                cur_proc->segv++;
                continue;
            }
        }
        if (!pte->valid) {
            frame = get_frame();
            if (pte->filemapped) {
                if (O)
                    printf(" FIN\n");
                cur_proc->fins++;
            } else if (pte->pageout) {
                if (O)
                    printf(" IN\n");
                cur_proc->ins++;
            } else {
                if (O)
                    printf(" ZERO\n");
                cur_proc->zeros++;
            }
            frame->bitcount = 0;
            frame->last_time = instr_cnt;
            frame->pte = pte;
            if (O)
                printf(" MAP %d\n", frame->id);
            cur_proc->maps++;
            pte->valid = true;
            pte->frame = frame->id;
        } else {
            auto ind = cur_proc->page_table[vpage]->frame;
            frame = frame_table[ind];
        }

        if (op == 'r')
            pte->referenced = true;
        else if (op == 'w') {
            pte->referenced = true;
            if (pte->write_protect) {
                cout << " SEGPROT" << endl;
                cur_proc->segprot++;
            }
            else {
                pte->modified = true;
            }
        }
        frame->process_id = cur_proc->id;
        frame->vpage = vpage;
        frame->is_mapped = true;
        pager->update(frame);
        instr = nullptr;
    }

    if(P){
        for(auto p:process_list){
            p->print_table();
            cout<<endl;
        }
    }
    if(F){
        printf("FT:");
        for(auto f:frame_table){
            if(f->is_mapped)
                printf(" %d:%d", f->process_id, f->vpage);
            else
                printf(" *");
        }
    }
    cout<<endl;
    if(S){
        for(auto p:process_list){
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   p->id, p->unmaps, p->maps, p->ins, p->outs, p->fins, p->fouts, p->zeros, p->segv, p->segprot);
            cost +=300*p->maps+400*p->unmaps+3100*p->ins+2700*p->outs+2800*p->fins+2400*p->fouts+140*p->zeros+340*p->segv+420*p->segprot;
        }
        cost += accesses + 130 *context_switchs +1250*proc_exits;
        printf("TOTALCOST %lu %lu %lu %llu %lu\n", instr_cnt, context_switchs, proc_exits, cost, sizeof(PTE));
    }
    return 0;
}