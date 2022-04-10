#include "MM.h"

void PTE::reset() {
    valid = 0;
    write_protect= 0;
    referenced = 0;
    modified = 0;
    pageout = 0;
    filemapped = 0;
    frame = 0;
    in_vma = 0;
}

void Frame::free_frame() {
    is_free = true;
    is_mapped = false;
    process_id = -1;
    vpage = -1;
    pte = nullptr;
    bitcount =0;
    last_time =0;
}

Process::Process(){
    for(auto & i : page_table){
        PTE *pte = new PTE();
        i=pte;
    }
    unmaps=0;
    maps=0;
    ins=0;
    outs=0;
    fins=0;
    fouts=0;
    zeros=0;
    segv=0;
    segprot=0;
}

VMA* Process::get_vpage(int vpage) {
    for(auto vma : vma_list){
        if(vpage>=vma->start&&vpage<=vma->end)
            return vma;
    }
    return nullptr;
}

bool Process::is_vpage_filemapped(int vpage) {
    VMA *vma = get_vpage(vpage);
    if (vma->filemapped)
        return true;
    else
        return false;
}

bool Process::is_vpage_protected(int vpage) {
    VMA *vma = get_vpage(vpage);
    if(vma->write_protect)
        return true;
    else
        return false;
}

void Process::print_table() {
    printf("PT[%d]:",id);
    for(int i=0;i<MAX_V_SPACE;i++){
        PTE *pte=page_table[i];
        if(!pte->valid){
            if(pte->pageout)
                printf(" #");
            else
                printf(" *");
        }
        else{
            printf(" %d:", i);
            if(pte->referenced)
                printf("R");
            else
                printf("-");
            if (pte->modified)
                printf("M");
            else
                printf("-");
            if (pte->pageout)
                printf("S");
            else
                printf("-");
        }
    }
}

void Pager::update(Frame *frame){
    auto it = find(queue.begin(),queue.end(),frame);
    if(it!=queue.end())
        return;
    else
        queue.push_back(frame);
}

Frame* FIFO::select_victim_frame() {
    if(queue.empty())
        return nullptr;
    Frame *frame = queue.front();
    queue.erase(queue.begin());
    return frame;
}

Frame* Rand::select_victim_frame(){
    int i = rnd->random(frame_cnt);
    Frame *frame = frame_table[i];
    return frame;
}

Frame* Clock::select_victim_frame() {
    Frame *ptr;
    while(!queue.empty()){
        if(hand == queue.size())
            hand=0;
        ptr = queue[hand];
        if(!ptr->pte->referenced ){
            hand++;
            return ptr;
        }
        else{
            ptr->pte->referenced = false;
            hand++;
        }
    }
    printf("ERROR: Clock is empty");
    exit(1);
}

void NRU::reset_bit(){
    for(auto & i : queue){
        i->pte->referenced=false;
    }
    time = instr_cnt;
}

Frame*NRU::select_victim_frame() {
    int victim_cand[4]={-1,-1,-1,-1};
    Frame * Class[4]={nullptr, nullptr, nullptr, nullptr};
    Frame *frame= nullptr;
    unsigned int temp = hand;
    for(int i=0;i<queue.size();i++){
        PTE* pte=queue[temp]->pte;
        if(Class[0]== nullptr&&!pte->referenced&&!pte->modified){
            Class[0]=queue[temp];
            victim_cand[0] = temp;
        }
        else if(Class[1]== nullptr&&!pte->referenced&&pte->modified){
            Class[1]=queue[temp];
            victim_cand[1] = temp;
        }
        else if(Class[2]== nullptr&&pte->referenced&&!pte->modified){
            Class[2]=queue[temp];
            victim_cand[2]=temp;
        }
        else if(Class[3]== nullptr&&pte->referenced&&pte->modified){
            Class[3]=queue[temp];
            victim_cand[3]=temp;
        }
        temp++;
        temp %=queue.size();
    }
    for(int i : victim_cand){
        if(i!=-1){
            frame=queue[i];
            hand = ++i;
            hand %= queue.size();
            break;
        }
    }

    if(instr_cnt-time>=50)
        reset_bit();
    return frame;
}

Frame* Aging::select_victim_frame() {
    Frame* frame;
    for(auto & i : queue) {
        frame = i;
        frame->bitcount = frame->bitcount>>1;
        frame->bitcount[31]=frame->pte->referenced;
        frame->pte->referenced=false;
    }
    auto temp_hand = hand;
    unsigned int ind = 0;
    unsigned long min =ULONG_MAX;
    unsigned long temp_counter =0;
    for(int i=0;i<queue.size();i++){
        frame=queue[temp_hand];
        temp_counter = frame->bitcount.to_ulong();
        if(temp_counter< min){
            min = temp_counter;
            ind = temp_hand;
        }
        temp_hand ++;
        if(temp_hand == queue.size())
            temp_hand=0;
    }
    frame = queue[ind];
    hand = ++ind;
    hand %= queue.size();
    return frame;
}

Frame* Working_Set::select_victim_frame() {
    Frame *frame;
    int min = INT_MAX;
    int ind = -1;

    for(auto &i:queue){
        frame =queue[hand];
        if(frame->pte->referenced){
            frame->last_time=instr_cnt;
            frame->pte->referenced=false;
        }
        else if(instr_cnt-frame->last_time>= 50){
            hand++;
            hand %=queue.size();
            return frame;
        }
        if(frame->last_time<min){
            min = frame->last_time;
            ind = hand;
        }
        hand ++;
        hand%=queue.size();
    }
    frame = queue[ind];
    hand = ++ind;
    hand%=queue.size();
    return frame;
}