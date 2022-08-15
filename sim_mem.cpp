
#include "sim_mem.h"
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int find_free_space(int* memory); // helper function that find the first free space in the memory
bool no_free_space(int* memory, int size); // helper function that check if memory is full
void move_to_swap(int num_of_pages, int text_size, int page_size, int swapfile_fd, page_descriptor** page_table, int process_id, int* );

char main_memory[MEMORY_SIZE];
int who_to_move = 0; // from where to take the data in the who_to_move_next array
int ind = 0; // where to put the data in the who_to_move_next array
int place_in_swap = 0;

sim_mem::sim_mem(char exe_file_name1[],char exe_file_name2[], char swap_file_name[], int
text_size, int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size, int num_of_proc)
{
    if(num_of_proc == 1){
        program_fd[0] = open(exe_file_name1, O_RDONLY, 0);
        if(program_fd[0] == -1){ // check for error
            perror("file is not exist\n");
            exit(1);
        }
        this->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, 0644); // open/create the swap file
        if(this->swapfile_fd == -1){ // check for error
            perror("error with opening the file\n");
            exit(1);
        }
        char buf[data_size+bss_size+heap_stack_size];
        for (int i = 0; i < data_size+bss_size+heap_stack_size; ++i) { // fill the buffer with '0'
            buf[i] = '0';
        }
        for (int i = 0; i < page_size*(num_of_pages - (text_size / page_size)); i++) { // fill the swap file with 0
            int check = write(swapfile_fd, buf, 1);
            if(check == -1){ // CHECK FOR ERROR
                perror("error with the write function\n");
                exit(1);
            }
        }
    }
    else if(num_of_proc == 2){ // if there is two processes
        program_fd[0] = open(exe_file_name1, O_RDONLY, 0);
        if(program_fd[0] == -1){ // check for error
            perror("file is not exist\n");
            exit(1);
        }
        program_fd[1] = open(exe_file_name2, O_RDONLY, 0);
        if(program_fd[1] == -1){ // check for error
            perror("file is not exist");
            exit(1);
        }
        this->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, 0644); // open/create the swap file
        if(this->swapfile_fd == -1){ // check for error
            perror("error with opening the file\n");
            exit(1);
        }
        char buf[data_size+bss_size+heap_stack_size];
        for (int i = 0; i < data_size+bss_size+heap_stack_size; ++i) { // fill the buffer with '0'
            buf[i] = '0';
        }
        for (int i = 0; i < 2*page_size*(num_of_pages - (text_size / page_size)); i++) { // fill the swap file with 0
            int check = write(swapfile_fd, buf, 1);
            // CHECK FOR ERROR
            if(check == -1){
                perror("error with the write function\n");
                exit(1);
            }
        }
    }
    for (int i = 0; i < MEMORY_SIZE; ++i) { // init maim memory
        main_memory[i] = '0';
    }
    // initializing the attributes
    this->text_size = text_size;
    this->data_size = data_size;
    this->bss_size = bss_size;
    this->heap_stack_size = heap_stack_size;
    this->num_of_pages = num_of_pages;
    this->page_size = page_size;
    this->num_of_proc = num_of_proc;

    availableLoc = (int*)malloc(sizeof (int)*(MEMORY_SIZE/page_size)); // save the free spots in the main memory
    for (int i = 0; i < (MEMORY_SIZE/page_size); ++i) { // init the array with 0
        availableLoc[i] = 0;
    }
    who_to_move_next = (int*)malloc(sizeof (int)*(MEMORY_SIZE)); // arr that save who to move first from main memory
    for (int i = 0; i < (MEMORY_SIZE); ++i) { // init the array with 0
        who_to_move_next[i] = 0;
    }

    page_table = (page_descriptor**) malloc(sizeof (page_descriptor*)*2); // creating the page table for each proc
    for (int i = 0; i < num_of_proc; ++i) {
        page_table[i] = (page_descriptor*) malloc(sizeof (page_descriptor)*num_of_pages);
        for (int j = 0; j < num_of_pages; ++j) { // fill the necessary attributes for each page
            page_table[i][j].D = 0;
            page_table[i][j].P = 1;
            page_table[i][j].V = 0;
            page_table[i][j].frame = -1;
            page_table[i][j].swap_index = -1;
        }
        for (int j = 0; j < (text_size/page_size); ++j) { // the text permission is always only read
            page_table[i][j].P = 0;
        }
    }
}

char sim_mem::load(int process_id, int address){ // the load function
    int loc;
    char theChar;
    int pageNumber = address / page_size; // the page number of the address
    int offset = address % page_size; // the offset of the address in the page

    if(page_table[process_id-1][pageNumber].V == 1){ // if the page is already in the main memory
        int place = page_table[process_id-1][pageNumber].frame;
        theChar = main_memory[(place*page_size)+offset];
        return theChar;
    }
    else{ // if the page is not in the memory (v==0)
        bool flag = no_free_space(availableLoc, (MEMORY_SIZE/page_size)); // return false if there is space, return true if there is no space
        if(page_table[process_id-1][pageNumber].P == 0){ // the page is in the exec
            if(!flag){ // if there is space
                loc = find_free_space((availableLoc));
            }
            else{ // if there is no space
                if(page_table[process_id-1][who_to_move_next[who_to_move]].V == 1 && page_table[process_id-1][who_to_move_next[who_to_move]].D == 1){ // check if we need to move to swap or not
                    move_to_swap(num_of_pages, text_size, page_size, swapfile_fd, page_table, process_id, who_to_move_next);
                    who_to_move++;
                    page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                    availableLoc[who_to_move] = 0;
                    loc = find_free_space((availableLoc));
                    ind++;
                }
                else{ // if we don't need to send to swap
                    loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                    page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
                    page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                }
            }
            lseek(program_fd[process_id-1], (pageNumber*page_size), SEEK_SET); // the currser location
            int check = read(program_fd[process_id-1], &main_memory[loc*page_size], page_size);
            if(check == -1){ // CHECK FOR ERROR
                perror("error with the read function\n");
                exit(1);
            }
            who_to_move_next[ind] = pageNumber;     // CHECK!
            ind++;
            theChar = main_memory[(loc*page_size)+offset];
            page_table[process_id-1][pageNumber].V = 1; // update
            page_table[process_id-1][pageNumber].frame = loc; //update the frame
            availableLoc[loc] = 1;
            return theChar;
        }
        else{ // the page is not in the text section (p==1)
            if(page_table[process_id-1][pageNumber].D == 0){ // not in swap
                if(address >= text_size && address < text_size + data_size) { //if data - get from the file and load to the main memory
                    if(!flag){ // if there is space
                        loc = find_free_space((availableLoc));
                    }
                    else{ // if there is no space
                        if(page_table[process_id-1][who_to_move_next[who_to_move]].V == 1 && page_table[process_id-1][who_to_move_next[who_to_move]].D == 1){ // check if we need to move to swap or not
                            move_to_swap(num_of_pages, text_size, page_size, swapfile_fd, page_table, process_id, who_to_move_next);
                            page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                            availableLoc[who_to_move] = 0;
                            loc = find_free_space((availableLoc));
                            who_to_move++;
                            ind++;
                        }
                        else{ // if we don't need to send to swap
                            loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                            page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
                            page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                        }
                    }
                    lseek(program_fd[process_id-1], (pageNumber*page_size), SEEK_SET); // the currser location
                    int check = read(program_fd[process_id-1], &main_memory[loc*page_size], page_size);
                    if(check == -1){ // CHECK FOR ERROR
                        perror("error with the read function\n");
                        exit(1);
                    }
                    who_to_move_next[ind] = pageNumber;
                    ind++;
                    theChar = main_memory[(loc*page_size)+offset];
                    page_table[process_id-1][pageNumber].V = 1; // update
                    page_table[process_id-1][pageNumber].frame = loc; //update the frame
                    availableLoc[loc] = 1;
                    return theChar;
                }
                else if(address >= text_size + data_size) { // if in bss, heap or stack - error
                    printf("ERROR - can't load page from this address\n");
                    theChar = '-';
                    return theChar;
                }
            }
            else{ // the page is in the swap (d==1)
                char buffer[page_size];
                for (int i = 0; i <page_size; ++i) { // fill the buffer with '0'
                    buffer[i] = '0';
                }
                if(!flag){ // if there is space in the main memory
                    loc = find_free_space((availableLoc));
                }
                else{ // if there is no space in the main memory
                    if(page_table[process_id-1][who_to_move_next[who_to_move]].V == 1 && page_table[process_id-1][who_to_move_next[who_to_move]].D == 1) { // check if we need to move to swap or not
                        move_to_swap(num_of_pages, text_size, page_size, swapfile_fd, page_table, process_id, who_to_move_next);
                        who_to_move++;
                        loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                    }
                    else{
                        loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                        page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
                        page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                    }
                }
                lseek(swapfile_fd, (page_table[process_id-1][pageNumber].swap_index)*page_size, SEEK_SET); // the currser location
                int check = read(swapfile_fd, &main_memory[loc*page_size], page_size);
                if(check == -1){ // CHECK FOR ERROR
                    perror("error with the read function\n");
                    exit(1);
                }
                lseek(swapfile_fd, (page_table[process_id-1][pageNumber].swap_index)*page_size, SEEK_SET); // the currser location
                int check2 = write(swapfile_fd, buffer, page_size); // put 0 where the page was in the swap file
                if(check2 == -1){// CHECK FOR ERROR
                    perror("error with the write function\n");
                    exit(1);
                }
                who_to_move_next[ind] = pageNumber;
                ind++;
                theChar = main_memory[(loc*page_size)+offset];
                page_table[process_id-1][pageNumber].V = 1; // update
                page_table[process_id-1][pageNumber].frame = loc; //update the frame
                page_table[process_id-1][pageNumber].swap_index = -1;
                availableLoc[loc] = 1;
                page_table[process_id-1][who_to_move_next[ind]].frame = -1;

                //NEED TO ADD - who_to_move--;

                return theChar;
            }
        }
    }
}

void sim_mem::store(int process_id, int address, char value){ // the store function

    int pageNumber = address / page_size; // the page number of the address
    int offset = address % page_size; // the offset of the address in the page

    if(page_table[process_id-1][pageNumber].V == 1){ // if the page is already in the main memory
        int place = page_table[process_id-1][pageNumber].frame;
        main_memory[(place*page_size)+offset] = value;
        page_table[process_id-1][pageNumber].D = 1;
    }
    else{ // if the page is not in the memory (V == 0)
        if(page_table[process_id-1][pageNumber].P == 0){ // the page is in the exec
            printf("can't write to the text location\n"); // can't change the text section
        }
        else{ // the page is not in the text section (P == 1)
            bool flag = no_free_space(availableLoc, (MEMORY_SIZE/page_size)); // return false if there is space, return true if there is no space
            int loc;
            if(page_table[process_id-1][pageNumber].D == 0){
                //split to data and else
                if(address >= text_size && address < text_size + data_size){ //if data - get from the file and load to the main memory
                    if(!flag){ // if there is space
                        loc = find_free_space((availableLoc));
                    }
                    else{ // if there is no space
                        if(page_table[process_id-1][who_to_move_next[who_to_move]].V == 1 && page_table[process_id-1][who_to_move_next[who_to_move]].D == 1){ // check if we need to move to swap or not
                            move_to_swap(num_of_pages, text_size, page_size, swapfile_fd, page_table, process_id, who_to_move_next);
                            who_to_move++;
                            page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                            availableLoc[who_to_move] = 0; // this place is now free
                            loc = find_free_space((availableLoc));
                            ind++;
                        }
                        else{ // if we don't need to send to swap
                            loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                            page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
                            page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                        }
                    }
                    lseek(program_fd[process_id-1], (pageNumber*page_size), SEEK_SET); // the currser location
                    int check = read(program_fd[process_id-1], &main_memory[loc*page_size], page_size);
                    if(check == -1){ // CHECK FOR ERROR
                        perror("error with the read function\n");
                        exit(1);
                    }
                    main_memory[(loc*page_size)+offset] = value;
                    page_table[process_id-1][pageNumber].V = 1; // update
                    page_table[process_id-1][pageNumber].D = 1; // update
                    page_table[process_id-1][pageNumber].frame = loc; //update the frame
                    availableLoc[loc] = 1;
                    who_to_move_next[ind] = pageNumber;
                    ind++;
                }
                else if(address >= text_size + data_size){ //else - init 0 in main memory
                    for (int i = 0; i < page_size; ++i) { // put 0 in the memory
                        main_memory[loc+i] = '0';
                    }
                    main_memory[(loc*page_size)+offset] = value; // store the value in the memory
                    page_table[process_id-1][pageNumber].V = 1; // update
                    page_table[process_id-1][pageNumber].D = 1; // update
                    page_table[process_id-1][pageNumber].frame = loc; //update the frame
                    availableLoc[loc] = 1;
                }
            }
            else{ // the page is in the swap (D == 1)
                char buffer[page_size];
                for (int i = 0; i <page_size; ++i) { // fill the buffer with '0'
                    buffer[i] = '0';
                }
                if(!flag){ // if there is space in the main memory
                    loc = find_free_space((availableLoc));
                }
                else{ // if there is no space in the main memory
                    if(page_table[process_id-1][who_to_move_next[who_to_move]].V == 1 && page_table[process_id-1][who_to_move_next[who_to_move]].D == 1) { // check if we need to move to swap or not
                        move_to_swap(num_of_pages, text_size, page_size, swapfile_fd, page_table, process_id, who_to_move_next);
                        who_to_move++;
                        loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                    }
                    else{ // if we can "run over" the page
                        loc = page_table[process_id-1][who_to_move_next[who_to_move]].frame;
                        page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
                        page_table[process_id-1][who_to_move_next[who_to_move]].frame = -1;
                    }
                }
                lseek(swapfile_fd, (page_table[process_id-1][pageNumber].swap_index)*page_size, SEEK_SET); // the currser location
                int check = read(swapfile_fd, &main_memory[loc*page_size], page_size);
                if(check == -1){ // CHECK FOR ERROR
                    perror("error with the read function\n");
                    exit(1);
                }
                lseek(swapfile_fd, (page_table[process_id-1][pageNumber].swap_index)*page_size, SEEK_SET); // the currser location
                int check2 = write(swapfile_fd, buffer, page_size); // put 0 where the page was in the swap file
                if(check2 == -1){// CHECK FOR ERROR
                    perror("error with the write function\n");
                    exit(1);
                }
                main_memory[(loc*page_size)+offset] = value;
                page_table[process_id-1][pageNumber].V = 1; // update
                page_table[process_id-1][pageNumber].D = 1;
                page_table[process_id-1][pageNumber].frame = loc; //update the frame
                page_table[process_id-1][pageNumber].swap_index = -1;
                availableLoc[loc] = 1;
                page_table[process_id-1][who_to_move_next[ind]].frame = -1;
                ind++;

                //NEED TO ADD - who_to_move--;
            }
        }
    }
}

void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for(i = 0; i < MEMORY_SIZE; i++) {
        printf("[%c]\n", main_memory[i]);
    }
}

void sim_mem::print_swap() {
    char* str = (char*)malloc(this->page_size *sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while(read(swapfile_fd, str, this->page_size) == this->page_size) {
        for(i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }
    free(str);
}

void sim_mem::print_page_table() {
    int i;
    for (int j = 0; j < num_of_proc; j++) {
        printf("\n page table of process: %d \n", j);
        printf("Valid\t Dirty\t Permission \t Frame\t Swap index\n");
        for(i = 0; i < num_of_pages; i++) {
            printf("[%d]\t[%d]\t[%d]\t[%d]\t[%d]\n",
                   page_table[j][i].V,
                   page_table[j][i].D,
                   page_table[j][i].P,
                   page_table[j][i].frame ,
                   page_table[j][i].swap_index);
        }
    }
}

sim_mem::~sim_mem() { // the distractor
    if(num_of_proc == 1){
        close(program_fd[0]);
        free(this->page_table[0]);
    }
    else if(num_of_proc == 2){
        close(program_fd[0]);
        close(program_fd[1]);
        free(this->page_table[0]);
        free(this->page_table[1]);
    }
    close(swapfile_fd);
    free(this->availableLoc);
    free(this->page_table);
    free(this->who_to_move_next);
}

int find_free_space(int* memory){ // helper function that find the first free space in the memory
    int index = 0;
    while(memory[index] != 0){
        index++;
    }
    return index;
}

bool no_free_space(int* memory, int size){ // helper function that check if there is a free space in the memory
    bool flag = true;
    for (int i = 0; i < size; ++i) {
        if(memory[i] == 0){
            flag = false;
        }
    }
    return flag;
}

void move_to_swap(int num_of_pages, int text_size, int page_size, int swapfile_fd, page_descriptor** page_table, int process_id, int* who_to_move_next) { // helper function that move page to the swap file
    if (place_in_swap > (num_of_pages - (text_size / page_size))) { // if there is place in the swap
        place_in_swap = 0;
    }
    lseek(swapfile_fd, (place_in_swap * page_size), SEEK_SET); // the currser location
    write(swapfile_fd, &main_memory[(page_table[process_id-1][who_to_move_next[who_to_move]].frame) * page_size],page_size);        // CHECK!
    page_table[process_id-1][who_to_move_next[who_to_move]].swap_index = place_in_swap;
    page_table[process_id-1][who_to_move_next[who_to_move]].V = 0;
    place_in_swap++;
}