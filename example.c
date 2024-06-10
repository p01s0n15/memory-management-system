#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdint.h>

#define PAGE_SIZE 4096

struct Segment {
    size_t size;
    int type;    
};

struct SubChainNode {
    struct Segment segment;
    struct SubChainNode* prev;
    struct SubChainNode* next;
};

struct MainChainNode {
    struct SubChainNode* sub_chain;
    struct MainChainNode* prev;
    struct MainChainNode* next;
};

struct MainChainNode* main_chain_head = NULL;

void mems_init() {
    
    main_chain_head = (struct MainChainNode*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (main_chain_head == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    main_chain_head->prev = NULL;
    main_chain_head->next = NULL;
    main_chain_head->sub_chain = NULL;
}
void* mems_malloc(size_t size) {
    size_t required_size = (size / PAGE_SIZE) * PAGE_SIZE;
    if (size % PAGE_SIZE != 0) {
        required_size += PAGE_SIZE;
    }

    void* mem_ptr = mmap(NULL, required_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    struct SubChainNode* sub_chain_node = (struct SubChainNode*)mmap(NULL, sizeof(struct SubChainNode), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (sub_chain_node == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    sub_chain_node->segment.size = required_size;
    sub_chain_node->segment.type = 1;
    sub_chain_node->prev = NULL;
    sub_chain_node->next = NULL;

    if (!main_chain_head->sub_chain) {
        main_chain_head->sub_chain = sub_chain_node;
    } else {
        struct SubChainNode* current_sub = main_chain_head->sub_chain;
        while (current_sub->next) {
            current_sub = current_sub->next;
        }
        current_sub->next = sub_chain_node;
        sub_chain_node->prev = current_sub;
    }

    return mem_ptr;
}

void mems_free(void* v_ptr) {
    struct MainChainNode* current_main = main_chain_head;
    while (current_main) {
        struct SubChainNode* current_sub = current_main->sub_chain;
        while (current_sub) {
            if (v_ptr == (void*)current_sub + sizeof(struct SubChainNode)) {
                current_sub->segment.type = 0; // 0 for HOLE
                break;
            }
            current_sub = current_sub->next;
        }
        current_main = current_main->next;
    }


}
// Function to read a value from a virtual address
void mems_get(void* v_ptr, int value) {
    printf("\n------Assigning value to virtual address [mems_get] ------\n");
    printf("Virtual address:%p ", v_ptr);

    // compute physical address
    uintptr_t physical_address = (uintptr_t)v_ptr - (uintptr_t)main_chain_head + PAGE_SIZE;

    printf("Physical address:%lu\n", physical_address);
    printf("value written:%d\n", value);
}



// Function to clean up the mems system and allocated memory.
void mems_finish() {
    struct MainChainNode* current_main = main_chain_head;
    while (current_main) {
        struct MainChainNode* next_main = current_main->next;
        struct SubChainNode* current_sub = current_main->sub_chain;
        while (current_sub) {
            struct SubChainNode* next_sub = current_sub->next;
            munmap(current_sub, current_sub->segment.size);
            current_sub = next_sub;
        }
        munmap(current_main->sub_chain, sizeof(struct SubChainNode));
        munmap(current_main, PAGE_SIZE);
        current_main = next_main;
    }
}

// Function to print statistics
void mems_print_stats() {
    struct MainChainNode* current_main = main_chain_head;
    size_t total_pages = 0;
    size_t total_used_memory = 0;
    size_t total_hole_memory = 0;

    printf("-----MeMS SYSTEM STATS-----\n");
    while (current_main) {
        printf("MAIN[%zu:%zu]-> ", (size_t)current_main, (size_t)current_main + PAGE_SIZE - 1);
        struct SubChainNode* current_sub = current_main->sub_chain;
        while (current_sub) {
            printf("%s[%zu:%zu] <-> ", (current_sub->segment.type == 0) ? "H" : "P", (size_t)current_sub, (size_t)current_sub + current_sub->segment.size - 1);
            total_pages += current_sub->segment.size / PAGE_SIZE;
            total_hole_memory += (current_sub->segment.type == 0) ? current_sub->segment.size : 0;
            total_used_memory += (current_sub->segment.type == 1) ? current_sub->segment.size : 0;
            current_sub = current_sub->next;
        }
        printf("NULL\n");
        current_main = current_main->next;
    }

    size_t main_chain_length = 0;
    struct MainChainNode* mc = main_chain_head;
    while (mc) {
        main_chain_length++;
        mc = mc->next;
    }

    printf("Pages used: %zu\n", total_pages);
    printf("Space unused: %zu\n", total_hole_memory);
    printf("Main Chain Length: %zu\n", main_chain_length);
    printf("Sub-chain Length array[");
    current_main = main_chain_head;
    while (current_main) {
        size_t sub_chain_length = 0;
        struct SubChainNode* current_sub = current_main->sub_chain;
        while (current_sub) {
            sub_chain_length++;
            current_sub = current_sub->next;
        }
        printf("%zu, ", sub_chain_length);
        current_main = current_main->next;
    }
    printf("]\n");

    printf("-------------------------\n");
}


int main(int argc, char const *argv[]) {
    mems_init();
    printf("------- Allocated virtual addresses [mems_malloc] -------\n");
    void* ptr[10];

    for (int i = 0; i < 10; i++) {
        ptr[i] = mems_malloc(sizeof(int) * 250);
        printf("Virtual address: %p\n", ptr[i]);
    }

    printf("--------- Printing Stats [mems_print_stats] --------\n");
    mems_print_stats();
    
    printf("------- Freeing up the memory [mems_free] --------\n");
    mems_free(ptr[3]);
    
    printf("--------- Printing Stats [mems_print_stats] --------\n");
    mems_print_stats();
    
    mems_get(ptr[0], 200);
    mems_get(ptr[1], 300);

    printf("------- Unmapping all memory[mems_finish] --------\n");
    mems_finish();
    return 0;
}
