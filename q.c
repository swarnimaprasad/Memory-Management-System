#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdint.h>  // Include this for uintptr_t

#define PAGE_SIZE 4096

enum SegmentType {
    HOLE,
    PROCESS
};

// Structure to represent a sub-chain node
struct SubChainNode {
    void* start_address;
    size_t size;
    enum SegmentType type;
    struct SubChainNode* prev;
    struct SubChainNode* next;
};

// Structure to represent a main-chain node
struct MainChainNode {
    struct SubChainNode* sub_chain;
    struct MainChainNode* prev;
    struct MainChainNode* next;
};

// Global variables to keep track of free list and MeMS virtual address
struct MainChainNode* free_list = NULL;
void* start_mems_virtual_address = NULL;

// Global variables to store statistics
int total_mapped_pages = 0;
size_t total_unused_memory = 0;

// Function to add a segment to the sub-chain
void add_segment(struct MainChainNode* main_chain, size_t size) {
    struct SubChainNode* newSubChainNode = (struct SubChainNode*)malloc(sizeof(struct SubChainNode));
    newSubChainNode->size = size;
    newSubChainNode->type = HOLE; // Initially, it's a HOLE segment

    if (main_chain->sub_chain == NULL) {
        newSubChainNode->start_address = start_mems_virtual_address;
        newSubChainNode->prev = newSubChainNode;
        newSubChainNode->next = newSubChainNode;
        main_chain->sub_chain = newSubChainNode;
    } else {
        newSubChainNode->start_address = (void*)((char*)main_chain->sub_chain->prev->start_address + main_chain->sub_chain->prev->size);
        newSubChainNode->prev = main_chain->sub_chain->prev;
        newSubChainNode->next = main_chain->sub_chain;
        main_chain->sub_chain->prev->next = newSubChainNode;
        main_chain->sub_chain->prev = newSubChainNode;
    }
}

// Initialize MeMS
void mems_init() {
    // Allocate initial memory for free_list
    free_list = (struct MainChainNode*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    free_list->sub_chain = NULL;
    free_list->prev = free_list;
    free_list->next = free_list;

    // Allocate memory for the start MeMS virtual address
    start_mems_virtual_address = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (free_list == MAP_FAILED || start_mems_virtual_address == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize statistics
    total_mapped_pages = 0;
    total_unused_memory = 0;

    printf("MeMS initialized\n");
}

// Cleanup MeMS
void mems_finish() {
    struct MainChainNode* currentNode = free_list;

    while (currentNode != free_list) {
        struct SubChainNode* subChainNode = currentNode->sub_chain;

        while (subChainNode != currentNode->sub_chain) {
            if (subChainNode->type == PROCESS) {
                munmap(subChainNode->start_address, subChainNode->size);
                printf("Deallocated %zu bytes at address %p\n", subChainNode->size, subChainNode->start_address);
            }

            struct SubChainNode* temp = subChainNode;
            subChainNode = subChainNode->next;
            free(temp);
        }

        struct MainChainNode* temp = currentNode;
        currentNode = currentNode->next;
        free(temp);
    }

    printf("MeMS finished\n");
}

// Allocate memory using MeMS
void* mems_malloc(size_t size) {
    struct MainChainNode* currentNode = free_list;

    while (currentNode != free_list) {
        struct SubChainNode* subChainNode = currentNode->sub_chain;

        while (subChainNode != currentNode->sub_chain) {
            if (subChainNode->type == HOLE && subChainNode->size >= size) {
                if (subChainNode->size > size + PAGE_SIZE) {
                    size_t remaining_size = subChainNode->size - size;
                    subChainNode->size = size;
                    subChainNode->type = PROCESS;

                    struct SubChainNode* newSubChainNode = (struct SubChainNode*)malloc(sizeof(struct SubChainNode));
                    newSubChainNode->start_address = (void*)((char*)subChainNode->start_address + size);
                    newSubChainNode->size = remaining_size;
                    newSubChainNode->type = HOLE;

                    newSubChainNode->prev = subChainNode;
                    newSubChainNode->next = subChainNode->next;
                    subChainNode->next->prev = newSubChainNode;
                    subChainNode->next = newSubChainNode;

                    total_mapped_pages += (size + PAGE_SIZE - 1) / PAGE_SIZE;
                    total_unused_memory += remaining_size;

                    printf("Allocated %zu bytes at address %p (Split from a larger HOLE)\n", size, subChainNode->start_address);
                } else {
                    subChainNode->type = PROCESS;

                    total_mapped_pages += (subChainNode->size + PAGE_SIZE - 1) / PAGE_SIZE;
                    total_unused_memory -= subChainNode->size;

                    printf("Allocated %zu bytes at address %p\n", size, subChainNode->start_address);
                }
                return subChainNode->start_address;
            }
            subChainNode = subChainNode->next;
        }
        currentNode = currentNode->next;
    }

    void* allocated_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocated_memory == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    add_segment(currentNode, size);

    total_mapped_pages += (size + PAGE_SIZE - 1) / PAGE_SIZE;
    total_unused_memory += size;

    printf("Allocated %zu bytes at address %p\n", size, allocated_memory);
    return allocated_memory;
}

// Print MeMS statistics
void mems_print_stats() {
    printf("--------- Printing Stats [mems_print_stats] --------\n");
    printf("Total Mapped Pages: %d\n", total_mapped_pages);
    printf("Total Unused Memory: %zu bytes\n", total_unused_memory);
    printf("\n");
}

// Get MeMS physical address from virtual address
void* mems_get(void* v_ptr) {
    return v_ptr; // The virtual address is the same as the physical address in our case
}

// Free memory using MeMS
// Free memory using MeMS
// Free memory using MeMS based on memory location
void mems_free(void* v_ptr) {
    struct MainChainNode* currentNode = free_list;

    while (1) { // Infinite loop to go through all nodes
        struct SubChainNode* subChainNode = currentNode->sub_chain;

        while (subChainNode != currentNode->sub_chain) {
            if (v_ptr == subChainNode->start_address && subChainNode->type == PROCESS) {
                // Your existing logic here

                return;
            }
            subChainNode = subChainNode->next;
        }

        currentNode = currentNode->next;

        if (currentNode == free_list) {
            // All nodes have been checked once, exit the loop
            break;
        }
    }

    fprintf(stderr, "Error: Attempted to free unallocated memory based on memory location.\n"); 