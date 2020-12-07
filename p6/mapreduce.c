// Copyright 2019 Jiaqi Wu, Runheng Lei
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "mapreduce.h"

#define INITIAL_CAPACITY 2
// here are two basic structure for my mapreduce
// one is the value node that holds a single value and link to another valuenode
// the second is the key node that not only hold the key,
// but also hold the linked head of the value node
typedef struct _ValueNode {
    char *value;
    struct _ValueNode *next;
} ValueNode;

typedef struct {
    char *key;
    ValueNode *valHead;
    // iterator
    ValueNode *currentNode;
} KeyNode;

typedef struct {
    pthread_mutex_t lock;
    // this has to be an array for qsort used later;
    KeyNode **keyNodes;
    int numOfKey;
    int numOfSpace;
} Partition;


void ReduceLaunch();

void *ReduceThread(void *partition_idx);

void sort_all(Partition **all_partitions);

int comparator(const void *p1, const void *p2);

void MR_Emit(char *key, char *value);

void expand(Partition *partition);

char *getter(char *key, int partition_number);

void *MapThread(void *index);

void MapLaunch();

// setting those as global variable
// for MRrun and other function to
// numOfFiles = argc -1;
int numOfFiles;
// argv[1] to argv[numOfFiles]
char **fileNames;
Mapper mapFunction;
int numOfMapperThread;
Reducer reduceFunction;
int numOfReducerThread;
int numOfPartition;
Partitioner partitionFunction;
// making an array for pointer, point to each partition
Partition **all_partitions;

void MR_Emit(char *key, char *value) {
    // using the partiton function to get the partition index
    int partitionIndex = partitionFunction(key, numOfPartition);
    Partition *partition = all_partitions[partitionIndex];
    pthread_mutex_lock(&(partition->lock));
    if (partition->numOfKey == partition->numOfSpace) {
        expand(partition);
    }
    // used to use linear placement, change to hashing
    // if the partition does not have the key yet,
    // adding the key to the key array
    // else if the partition already has the key,
    // simply add the value to the top of the value array
    // First check if the key is in the partition yet
    // int i;
    // int index = partition->numOfKey;
    // for(i = 0;i<partition->numOfKey;i++){
    // compare the key with the entire array, if found, change index
    //    if(strcmp(partition->keyNodes[i]->key,key)==0){
    //        index = i;
    //        break;
    //    }
    // }
    int hashIndex = MR_DefaultHashPartition(key, partition->numOfSpace);
    int probe;
    // get the current Key in the matching index
    KeyNode *currentKey = partition->keyNodes[hashIndex];
    if (currentKey == NULL) {  // if not exist yet, make a new one
        // setting up key node and value node
        KeyNode *key_node = (KeyNode *) malloc(sizeof(KeyNode));
        key_node->key = strdup(key);
        ValueNode *val_node = (ValueNode *) malloc(sizeof(ValueNode));
        val_node->value = strdup(value);
        val_node->next = NULL;
        key_node->valHead = val_node;
        key_node->currentNode = key_node->valHead;
        // put it on the position and increment the count;
        partition->keyNodes[hashIndex] = key_node;
        partition->numOfKey++;
        // unlock and return
        pthread_mutex_unlock(&(partition->lock));
        return;
    } else if (strcmp(currentKey->key, key) == 0) {
        // if the key is alreayd in the keynodes array
        // set up a value node
        ValueNode *val_node = (ValueNode *) malloc(sizeof(ValueNode));
        val_node->value = strdup(value);
        // put the new value node to the top of the linked list
        val_node->next = currentKey->valHead;
        currentKey->valHead = val_node;
        currentKey->currentNode = currentKey->valHead;
        pthread_mutex_unlock(&(partition->lock));
        return;
    }
    // collision happened
    // using probe as the new index, increment the number by 1
    probe = (hashIndex + 1) % partition->numOfSpace;
    while (probe != -1 && probe != hashIndex) {
        KeyNode *get_node = partition->keyNodes[probe];
        if (get_node == NULL) {  // no key in this slot
            // setting up key node and value node
            KeyNode *key_node = (KeyNode *) malloc(sizeof(KeyNode));
            key_node->key = strdup(key);
            ValueNode *val_node = (ValueNode *) malloc(sizeof(ValueNode));
            val_node->value = strdup(value);
            val_node->next = NULL;
            key_node->valHead = val_node;
            key_node->currentNode = key_node->valHead;
            // put it on the position and increment the count;
            partition->keyNodes[probe] = key_node;
            partition->numOfKey++;
            probe = -1;
        } else if (strcmp(get_node->key, key) == 0) {  // found the right key
            // if the key is alreayd in the keynodes array
            // set up a value node
            ValueNode *val_node = (ValueNode *) malloc(sizeof(ValueNode));
            val_node->value = strdup(value);
            // put the new value node to the top of the linked list
            val_node->next = get_node->valHead;
            get_node->valHead = val_node;
            get_node->currentNode = get_node->valHead;
            probe = -1;
        } else {
            // keep finding the next empty space to avoid collison
            probe = (probe + 1) % (partition->numOfSpace);
        }
    }
    if (probe != -1) {
        printf("need more space for hash");
        exit(1);
    }
    pthread_mutex_unlock(&(partition->lock));
    return;
}

void expand(Partition *partition) {
    // expand the partition by doubling the size
    // also tried realloc this, does not change the running time
    int newCapacity = partition->numOfSpace * 2;
    KeyNode **new_key_nodes = (KeyNode **)
            malloc(newCapacity * sizeof(KeyNode *));
    for (int i = 0; i < newCapacity; i++) {
        new_key_nodes[i] = NULL;
    }
    KeyNode **old_key_nodes = partition->keyNodes;
    int oldCapacity = partition->numOfSpace;

    partition->keyNodes = new_key_nodes;
    partition->numOfSpace = newCapacity;
    for (int i = 0; i < oldCapacity; i++) {
        // used to use linear placement, change to hashing
        // get the old key
        // KeyNode* key_node = old_key_nodes[i];
        // if the key is not null
        // if (key_node != NULL){
        // get the current key at that position
        //  KeyNode* get_node = partition->keyNodes[i];
        // if that is null, put the old key there
        // if (get_node == NULL){
        //   partition->keyNodes[i] = key_node;
        // }
        // rehashing the code in to the index
        KeyNode *key_node = old_key_nodes[i];
        if (key_node != NULL) {
            // realloct the old nodes
            int hashIndex = MR_DefaultHashPartition(key_node->key, newCapacity);
            // check if the node is in the new key array
            KeyNode *get_node = partition->keyNodes[hashIndex];
            if (get_node == NULL) {
                partition->keyNodes[hashIndex] = key_node;
                continue;
            }
            int probe = (hashIndex + 1) % newCapacity;;
            // rehashing if collison
            while (probe != -1 && probe != hashIndex) {
                KeyNode *get_node = partition->keyNodes[probe];
                if (get_node == NULL) {
                    partition->keyNodes[probe] = key_node;
                    probe = -1;
                } else {
                    // keep hashing
                    probe = (probe + 1) % newCapacity;;
                }
            }
            if (probe != -1) {
                printf("fail to expand");
                exit(1);
            }
        }
    }
    // free
    free(old_key_nodes);
    return;
}

// making map thread
// calling the actual function of mapper,
// a single map Thread can work on zero, one or more input files
void *MapThread(void *index) {
    // convert the index into int, and finding the file need to be map
    // the casting here is tricky,
    // cast it to the int pointer and then dereference
    int fileIndex = *((int *) index);
    while (fileIndex <= numOfFiles) {
        char *currentFile = fileNames[fileIndex];
        // typedef void (*Mapper)(char *file_name);
        mapFunction(currentFile);
        fileIndex += numOfMapperThread;
    }
    return NULL;
}

// launch the map function
// using in the MR Run function,
// creating map thread equal to the pass in argument
// and thread join
void MapLaunch() {
    int i;
    int *startIndex;
    int kArrLen = numOfMapperThread+1;
    pthread_t mapThreads[kArrLen];
    // while number of file starting with argv[1],
    // index should be match, so map thread also starting at index i
    // both argv[0] and MapThreads[0] are simply unused
    for (i = 1; i <= numOfMapperThread; i++) {
        startIndex = (int *) malloc(sizeof(int));
        *startIndex = i;
        // int pthread_create(pthread_t *thread,
        // const pthread_attr_t *attr,
        // void *(*start_routine) (void *), void *arg);
        // 0 for create success
        if (pthread_create(&mapThreads[i], NULL,
                           MapThread, (void *) startIndex) != 0) {
            // error message
            printf("Error Creating Thread\n");
            exit(1);
        }
    }
    // join after ward to avoid race condition
    // int pthread_join(pthread_t thread, void **retval);
    for (i = 1; i <= numOfMapperThread; i++) {
        pthread_join(mapThreads[i], NULL);
    }
    return;
}

// this is the get next method, keep getting until no more value
char *getter(char *key, int partition_number) {
    // find the current partition
    Partition *current = all_partitions[partition_number];
    // found the key node in that partition
    KeyNode *keynode = NULL;
    int i;
    int front = 0;
    int back = current->numOfKey - 1;
    int index;
    // linear search
    // for (i=0;i<current->numOfKey;i++){
    //    if(strcmp(current->keyNodes[i]->key,key)==0){
    // keyNode is array of key nodes pointer
    //        keynode = current->keyNodes[i];
    //        break;
    //    }
    // }

    // binary search
    while (front <= back) {
        index = (front + back) / 2;
        if (strcmp(current->keyNodes[index]->key, key) == 0) {
            keynode = current->keyNodes[index];
            break;
        } else if (strcmp(current->keyNodes[index]->key, key) > 0) {
            back = index - 1;
        } else {
            front = index + 1;
        }
    }
    if (keynode == NULL) {
        return NULL;
    } else {
        // if no more value, return null, else
        // pop the value from the key list, set the key to the next one.
        if (keynode->currentNode == NULL) {
            return NULL;
        }
        // move the iterator to the next node
        char *value = keynode->currentNode->value;
        keynode->currentNode = keynode->currentNode->next;
        return value;
    }
}

// this method is making the function pointer for reduce thread
void *ReduceThread(void *partitionIndex) {
    int partIndex = *((int *) partitionIndex);

    while (partIndex < numOfPartition) {
        // printf("partIndex %d \n" , partIndex);
        Partition *part = all_partitions[partIndex];
        // reduce all the key in the partition
        int i = 0;
        while (part->keyNodes[i] != NULL) {
            reduceFunction(part->keyNodes[i]->key, getter, partIndex);
            i++;
        }
        partIndex += numOfReducerThread;
    }
    return NULL;
}

// the structure here is almost the same as Mapper
// making a thread array, creating
void ReduceLaunch() {
    int *partitionIndex;
    int i;
    int kArrLen = numOfReducerThread;
    pthread_t reduceThreads[kArrLen];
    for (i = 0; i < numOfReducerThread; i++) {
        partitionIndex = (int *) malloc(sizeof(int));
        *partitionIndex = i;
        // 0 for create success
        if (pthread_create(&reduceThreads[i], NULL,
                           ReduceThread, (void *) partitionIndex) != 0) {
            // error message
            printf("Error Creating Thread\n");
            exit(1);
        }
    }
    for (i = 0; i < numOfReducerThread; i++) {
        pthread_join(reduceThreads[i], NULL);
    }
    return;
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

unsigned long MR_SortedPartition(char *key, int num_partitions) {
    if (num_partitions == 1) {
        return 0;
    }

    unsigned long partitionBits = 0;

    unsigned long keyBinary = strtoul(key, NULL, 10);
    // count how many bits in partition
    while (num_partitions) {
        num_partitions = num_partitions >> 1;
        partitionBits += 1;
    }
    partitionBits -= 1;
    unsigned long index = (keyBinary >> (32 - partitionBits));
    // return that many bits in key binary
    return index;
}

// using for qsort later, lexicographical order
int comparator(const void *p1, const void *p2) {
    KeyNode *ps1 = *((KeyNode **) p1);
    KeyNode *ps2 = *((KeyNode **) p2);
    if (ps1 == NULL)
        return 1;
    else if (ps2 == NULL)
        return -1;
    else
        return strcmp(ps1->key, ps2->key);
}


void free_mem() {
    // using pointer iterator to free everything
    Partition *partition;
    for (int i = 0; i < numOfPartition; i++) {
        partition = all_partitions[i];
        for (int j = 0; j < partition->numOfKey; j++) {
            KeyNode *currentKey = partition->keyNodes[j];
            ValueNode *currentValue = currentKey->valHead;
            while (currentValue != NULL) {
                ValueNode *tempNode = currentValue->next;
                free(currentValue);
                currentValue = tempNode;
            }
            free(currentKey);
        }
        free(partition);
    }
    free(all_partitions);
}


void MR_Run(int argc, char *argv[],
            Mapper map, int num_mappers,
            Reducer reduce, int num_reducers,
            Partitioner partition, int num_partitions) {
    // setting up all my variables
    numOfFiles = argc - 1;
    fileNames = argv;
    mapFunction = map;
    numOfMapperThread = num_mappers;
    reduceFunction = reduce;
    numOfReducerThread = num_reducers;
    partitionFunction = partition;
    numOfPartition = num_partitions;

    // init partitions
    // all partition is array of pointer to partition

    all_partitions = malloc(sizeof(Partition *) * numOfPartition);
    for (int i = 0; i < numOfPartition; i++) {
        all_partitions[i] = malloc(sizeof(Partition));
        pthread_mutex_init(&(all_partitions[i]->lock), NULL);
        // make it inital capacity
        all_partitions[i]->keyNodes = malloc(
                INITIAL_CAPACITY * sizeof(KeyNode *));
        for (int j = 0; j < INITIAL_CAPACITY; j++) {
            all_partitions[i]->keyNodes[j] = NULL;
        }
        all_partitions[i]->numOfKey = 0;
        all_partitions[i]->numOfSpace = INITIAL_CAPACITY;
    }
    MapLaunch();
    // sort all
    for (int i = 0; i < numOfPartition; i++) {
        qsort(all_partitions[i]->keyNodes, all_partitions[i]->numOfSpace,
              sizeof(KeyNode *), comparator);
    }
    ReduceLaunch();
    free_mem();
}


