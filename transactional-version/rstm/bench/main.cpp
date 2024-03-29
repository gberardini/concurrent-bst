#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <fstream>
#include <chrono> 

#include <api/api.hpp>
#include <common/platform.hpp>
#include <common/locks.hpp>
#include "bmconfig.hpp"
#include "../include/api/api.hpp"


#include <list> 
#include <iterator> 


#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex>
#include <thread>


#define SILENT



#define synchronized(m) \
    m.lock();           \
    for (int ___ct = 0; ___ct++ < 1; m.unlock())

using namespace std;
using namespace std::chrono; 



const int THREAD_COUNT = 4;



Config::Config() : bmname(""),
                   duration(1),
                   execute(2),
                   threads(THREAD_COUNT),
                   nops_after_tx(0),
                   elements(1000),
                   lookpct(50),
                   inspct(50),
                   sets(10),
                   ops(1),
                   time(0),
                   running(true),
                   txcount()
{
}

Config CFG TM_ALIGN(64);

void setConfigRSTM(Config &cfg,
               int numThreads, 
               int elements, 
               int lookpct, 
               int inspct, 
               int transactionSize) {
    cfg.bmname = "";
    cfg.duration = 1;
    cfg.execute = 2;
    cfg.threads = numThreads;
    cfg.nops_after_tx = 0;
    cfg.elements = elements;
    cfg.lookpct = lookpct;
    cfg.inspct = inspct;
    cfg.sets = 10;
    cfg.ops = transactionSize;
    cfg.time = 0;
    cfg.running = true;
    
}

// shared variable that will be incremented by transactions
int x = 0;

mutex printlock;

void printlocked(const char *s)
{
#ifndef SILENT
    printf("one is defined ");
    printlock.lock();
    printf(s);
    printlock.unlock();
#endif
}

using namespace std;

struct node
{
    int key_value;
    node *left;
    node *right;
};

class btree
{
  public:
    btree();
    ~btree();

    bool contains(int key);
    void add(int key);
    node *search(int key);
    void destroy_tree();

    int size()
    {
        return size(this->root);
    }
    int size(node *n)
    {

        if (n == NULL)
            return 0;
        return 1 + size(n->left) + size(n->right);
    }

    void inorderPrintRec(node *root)
    {

        if (root == NULL)
        {

            return;
        }
        inorderPrintRec(root->left);
        printf("%d ", root->key_value);
        inorderPrintRec(root->right);
    }

    void inorderPrint()
    {

        if (this->root == NULL)
        {
            printf("[ empty ] \n");
        }
        inorderPrintRec(this->root);
        printf("\n");
    }

    bool remove(int key)
    {

        TM_BEGIN(atomic) {

            // printf("calling remove");
            node *current = TM_READ(this->root);
            node *prev = NULL;
            bool leftLast = false;       
            while(true) {

                if (current == NULL) {
                    stm::commit(tx);
                    return false;
                }

                if (key < TM_READ(current->key_value)) { 
                    prev = current;
                    current = TM_READ(current->left);
                    leftLast = true;
                    continue;
                }
                if (key > TM_READ(current->key_value)) { 
                    prev = current;
                    current = TM_READ(current->right);
                    leftLast = false;
                    continue;
                }
                
                // KEY FOUND
                //no child case
                if (TM_READ(current->left) == NULL && TM_READ(current->right) == NULL)
                {
                    if (prev == NULL) {
                        // SOLE ROOT TO BE DELETED
                        TM_WRITE(this->root, (node*)NULL);
                        break;
                
                    }
                    else {
                        if(leftLast) TM_WRITE(prev->left, (node*)NULL);
                        else TM_WRITE(prev->right, (node*)NULL);
                        break;
                    }
                }

                //one child case
                if (TM_READ(current->left) == NULL)
                {
                    if (prev == NULL) {
                        TM_WRITE(this->root, this->root->right);
                        break;
                    }
                    else {
                        if(leftLast) TM_WRITE(prev->left, current->right);
                        else TM_WRITE(prev->right, current->right);
                        break;
                    };
                }

                //one child case
                if (TM_READ(current->right) == NULL)
                {
                    if (prev == NULL) {
                        TM_WRITE(this->root, this->root->left);
                        break;
                    }
                    else {
                        if(leftLast) TM_WRITE(prev->left, current->left);
                        else TM_WRITE(prev->right, current->left);
                        break;
                    };
                }

                

                //two child removal case 1
                if (TM_READ(current->right->left) == NULL)
                {
                    //printf("case1");
                    TM_WRITE(current->key_value, current->right->key_value);
                    TM_WRITE(current->right, current->right->right);
                    break;
                }

                //two child removal case 2
                //printf("case2");
                node *parent = TM_READ(current->right);
                node *child = TM_READ(current->right->left);

                while (TM_READ(child->left) != NULL)
                {
                    //printf("case2");
                    parent = child;
                    child = TM_READ(child->left);
                }

                //printf("Cambio puntatori\n");
                TM_WRITE(current->key_value, child->key_value);
                //printf("Faccio puntare %d  ", parent->left->key_value);
                //printf("%d",child->right->key_value);
                TM_WRITE(parent->left, child->right);
                break;
            }
        }
        TM_END;

        return true;
    }

  private:
    void destroy_tree(node *leaf);
    void add(int key, node *leaf);
    node *search(int key, node *leaf);

    node *root;
};

btree::btree()
{
    root = NULL;
}

btree::~btree()
{
    //destroy_tree();
}

void btree::destroy_tree()
{
    //destroy_tree(root);
}

bool btree::contains(int key) {
    TM_BEGIN(atomic) {
        node *current = TM_READ(this->root);
        
        while(current != NULL) {
            int curVal = TM_READ(current->key_value); 
            
            if (key == curVal) {
                stm::commit(tx);
                return true;
            }
            else if (key < curVal) {
                current = TM_READ(current->left);
            }
            else {
                current = TM_READ(current->right);
            }
        }

    }
    TM_END;

    return false;
}

void btree::add(int key)
{

    TM_BEGIN(atomic) {
        // if tree is empty
        if(TM_READ(this->root) == NULL) {
            node *newNode = new node;
            newNode->key_value = key;
            newNode->left = NULL;
            newNode->right = NULL;
            
            TM_WRITE(this->root, newNode);
        }
        else {
            node *current = TM_READ(this->root);
            while(true) {
                if (key < TM_READ(current->key_value))
                {
                    if (TM_READ(current->left) != NULL) {
                        current = TM_READ(current->left);
                        continue;
                    }
                    else
                    {
                        node *newNode = new node;
                        newNode->key_value = key;
                        newNode->left = NULL;
                        newNode->right = NULL;
                        
                        TM_WRITE(current->left, newNode);
                        break;
                    }
                }
                else if (key > TM_READ(current->key_value))
                {
                    if (current->right != NULL) {
                        current = TM_READ(current->right);
                        continue;
                    }
                    else
                    {
                        node *newNode = new node;
                        newNode->key_value = key;
                        newNode->left = NULL;
                        newNode->right = NULL;
                        
                        TM_WRITE(current->right, newNode);
                        break;
                    }
                }
                else {
                    break;
                }

            }   
        }
    }TM_END;
}
/*
void btree::add(int key)
{
    // TM_BEGIN(atomic) {
    //     if (root != NULL)
    //         add(key, root);
    //     else
    //     {
    //         root = new node;
    //         root->key_value = key;
    //         root->left = NULL;
    //         root->right = NULL;
    //     }
    // }
    // TM_END;
    bool firstNode = false;
    TM_BEGIN(atomic) {
        if(TM_READ(this->root) == NULL) {
            node *newNode = new node;
            newNode->key_value = key;
            newNode->left = NULL;
            newNode->right = NULL;
            TM_WRITE(this->root, newNode);

            firstNode = true;

        }
    }
    TM_END;
    
    if(!firstNode)
        add(key, this->root);
}*/

//deletion bottom up
void btree::destroy_tree(node *leaf)
{
    if (leaf != NULL)
    {
        destroy_tree(leaf->left);
        destroy_tree(leaf->right);
        delete leaf;
    }
}

class Dummy
{

  public:
    void add(int val)
    {

        printf("dummy %d \n", val);
    }
};



class TestCase
{

  public:
    int n;
    btree *tree = new btree();

    //   Dummy* tree = new Dummy();
    int *array;
    int nThreads;
    int percAdd, percRemove, percContains;
    int transactionSize;
    char *filename;

    struct op {
        char type; // C - contains/ R- remove/ A - add
        int arg; // value to be added or removed
    };

    list<op*> *createOpList(int numOperations) {
        list<op*> *newList = new list<op*>();

        for (int i = 0; i < numOperations; i++) {
            newList->push_back(createOpRandomly());
        }

        return newList;
    } 
    op* createOpRandomly() {
        int randNum = rand() % 100;

        char operation = 'A';
        if (randNum < percAdd) {
            operation = 'A';
        }
        else if (randNum < percAdd + percContains) {
            operation = 'C';
        }
        else {
            operation = 'R';

        }
        
        op* o = new op {
            operation,
            rand()%n
        };
        
        return o;
    }

    TestCase(char* filename, int numElements, int nThreads, int percAdd, int percRemove, int percContains, int transactionSize)
    {
        this->filename = filename;
        this->n = numElements;
        this->nThreads = nThreads;
        this->percAdd = percAdd;
        this->percRemove = percRemove;
        this->percContains = percContains;

        this->transactionSize = transactionSize;
        array = new int[n];
        srand(time(NULL));
        populateArray();
        
    }

    void run2() {
        thread workers[nThreads];
        int size = n / nThreads;
        
        list<op*>* opList[nThreads];
        for(int i =0 ;i < nThreads; i++) {
            opList[i] = createOpList(size);
        }
        
        auto start = high_resolution_clock::now(); 

        for(int i = 0 ;i < nThreads; i++) {
            workers[i] = thread(&TestCase::work, this, tree, opList[i]);
        }
        
        for(int i = 0; i < nThreads; i++) {
            workers[i].join();
         }
        auto stop = high_resolution_clock::now(); 
        auto duration = duration_cast<microseconds>(stop - start); 


        //printf("Tree size: %d\n", tree->size());

        outputToFile(filename, duration.count(),
                     this->n, 
                     this->nThreads,
                     this->percAdd,
                     this->percRemove,
                     this->percContains,
                     this->transactionSize);
    }
    // ~TestCase(){
    //     tree->deleteNodes();
    //     delete array;

    // }
//
    void populateArray()
    {

        for (int i = 0; i < n; i++)
        {
            array[i] = i;

            //printf("%d, ", array[i]);
        }
        //  int k = 0 ;
        for (int i = 0; i < n; i++)
        {

            int tmp = array[i];
            int r = rand() % n;
            array[i] = array[r];
            array[r] = tmp;
        }

        //for( int i = 0; i < n; i++ ){

        //     printf("%d, ", array[i]);

        // }

        //printf("\n");
    }

    void populateTree() {
        for (int i = 0; i < n/2; i++) {
            tree->add(rand()%n);
        }
    }

    void printList(list<op> *listOps) {
        for(list<op>::iterator it = listOps->begin(); it != listOps->end(); ++it ) {
            printf("%c - %d, ", it->type, it->arg);
        }

        printf("\n");
    }
    void work(btree *tree, list<op*> *listOps) {
        //printList(listOps);
        
        TM_THREAD_INIT();
        for(list<op*>::iterator it = listOps->begin(); it != listOps->end(); ++it ) {
            //printf("%p tree \n", tree);
            //printf("%c - %d, ", it->type, it->arg);
            op *operaton = *it;
            if (operaton->type == 'A') {
                tree->add(operaton->arg);
 
            }
            else if (operaton->type == 'R') {
                tree->remove(operaton->arg); 
            }
            else { //contains
                tree->contains(operaton->arg);
            }
        }
        TM_THREAD_SHUTDOWN();

        
    }
    void adder(btree *tree, int *array, int begin, int end)
    {
        // printf("%d, %d\n", begin, end);

        // init thread for rstm
        TM_THREAD_INIT();

        while (begin < end)
        {
            printlocked("adding\n");
            tree->add(array[begin]);
            //tree->add(5);
            begin++;
        }
        TM_THREAD_SHUTDOWN();
    }
    void remover(btree *tree, int *array, int begin, int end)
    {
        //printf("%d, %d\n", begin, end);
        TM_THREAD_INIT();

        while (begin < end)
        {

            printlocked("removing\n");
            int maxRetry = 5;
            while(!tree->remove(array[begin]) && maxRetry >= 0) {
                --maxRetry;//printf("trying to remove %d \n", array[begin]);
                sleep_ms(2*(1 + 5-maxRetry));
            }

            //tree->remove(array[begin]);
            begin++;
        }
        TM_THREAD_SHUTDOWN();
    }


    void outputToFile(char* filename, int64_t duration,
                        int elements, int nThreads,
                        int percAdd,
                        int percRemove,
                        int percContains, 
                        int transactionSize) {
        char *delimiter = ", ";

        ofstream outfile;
        outfile.open(filename, ios_base::app);
        outfile << duration << delimiter;
        outfile << elements << delimiter;
        outfile << nThreads << delimiter;
        
        outfile << percAdd << delimiter;
        outfile << percRemove << delimiter;
        outfile << percContains<< delimiter;

        
        outfile << transactionSize << delimiter;

        outfile << endl;
    }
    void run() 
    {
        
        thread workersAdd[nThreads];
        int size = n / nThreads;
        for (int i = 0; i < nThreads; i++)
        {

            workersAdd[i] = thread(&TestCase::adder, this, tree, array, i * size, (i + 1) * size);
        }
        for (int i = 0; i < nThreads; i++)
        {
            workersAdd[i].join();
        }
        

        printf("size before remove: %d\n",tree->size()) ;
        printf("starting removals\n\n");

//        return;
        thread workersRemove[nThreads];
/*        workersRemove[0] = thread(&TestCase::remover, this, tree, array, 0, n+1);
        workersRemove[0].join();
  */      
        
        
        for (int i = 0; i < nThreads; i++)
        {

            workersRemove[i] = thread(&TestCase::remover, this, tree, array, i * size, (i + 1) * size);
        }
        
    
        for (int i = 0; i < nThreads; i++)
        {
            workersRemove[i].join();
        }
        
        //tree->inorderPrint();
        printf("size after removals: %d\n", tree->size());
    
    } //run
};


/*
void* run_thread(void* i) {
    // each thread must be initialized before running transactions
    TM_THREAD_INIT();

    for(int i=0; i<NUM_TRANSACTIONS; i++) {
        // mark the beginning of a transaction
        TM_BEGIN(atomic)run2
        
        {
            // add this memory location to the read set
            int z = TM_READ(x);
            // add this memory location to the write set
            TM_WRITE(x, z+1);
        }
        TM_END; // mark the end of the transaction
    }

    TM_THREAD_SHUTDOWN();
}

*/

int main(int argc, char **argv)
{
    if (argc < 8) {
        printf("insufficient arguments\n");
        return 1;
    }
    
    // num elements
    int numElements = atoi(argv[1]);
    // num threads
    int numThreads = atoi(argv[2]);
    // percAdd
    int percAdd = atoi(argv[3]);
    //percRemove
    int percRemove = atoi(argv[4]);
    // percContains
    int percContains = atoi(argv[5]);
    //trans size
    int transactionSize = atoi(argv[6]);
    //output filename
    char *fname = argv[7];

    setConfigRSTM(CFG, numThreads, numElements, percContains, percAdd+percRemove, transactionSize);

    TM_SYS_INIT();
    TM_THREAD_INIT();

    TestCase test = TestCase(fname, numElements, numThreads, percAdd, percRemove, percContains, transactionSize);
    test.run2();

    TM_THREAD_SHUTDOWN();
    TM_SYS_SHUTDOWN();
    return 0;
}
