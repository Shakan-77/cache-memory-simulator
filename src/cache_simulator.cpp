#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>

using namespace std;

class CPUReq {
public:
    string type;
    int address;
    int data;
};

enum ReplacementPolicy {
    RANDOM_POLICY,
    LRU_POLICY,
    LIFO_POLICY
};

class CacheBlock {
public:
    string state;     // I, V, DIRTY
    int tag;
    int insertTime;
    int lastUsed;
    vector<int> data;

    CacheBlock(int blockSize = 0) {
        state = "I";
        tag = -1;
        insertTime = 0;
        lastUsed = 0;
        data = vector<int>(blockSize, 0);
    }
};

class DirectMappedCache {
public:
    vector<CacheBlock> cache;
    string writePolicy;
    int numBlocks;
    int blockSize;

    DirectMappedCache(int blocks, string wp, int wordsPerBlock) {
        numBlocks = blocks;
        blockSize = wordsPerBlock;
        writePolicy = wp;
        cache = vector<CacheBlock>(numBlocks, CacheBlock(blockSize));
    }

    int getIndex(int addr) {
        return (addr / (4 * blockSize)) % numBlocks;
    }

    int getTag(int addr) {
        return (addr / (4 * blockSize)) / numBlocks;
    }

    void writeback(int idx, vector<int>& memory) {
        int base = (cache[idx].tag * numBlocks + idx) * blockSize;
        for (int i = 0; i < blockSize; i++)
            memory[base + i] = cache[idx].data[i];
    }

    void loadBlock(int idx, int tag, vector<int>& memory) {
        int base = (tag * numBlocks + idx) * blockSize;
        for (int i = 0; i < blockSize; i++)
            cache[idx].data[i] = memory[base + i];

        cache[idx].tag = tag;
        cache[idx].state = "V";
    }

    void serviceRequest(CPUReq req, vector<int>& memory, int& hit, int& miss) {

        int idx = getIndex(req.address);
        int tag = getTag(req.address);
        int offset = (req.address / 4) % blockSize;

        CacheBlock& block = cache[idx];

        if (block.state != "I" && block.tag == tag) {
            hit++;
        } else {
            miss++;

            if (block.state == "DIRTY")
                writeback(idx, memory);

            loadBlock(idx, tag, memory);
        }

        if (req.type == "WRITE") {
            block.data[offset] = req.data;

            if (writePolicy == "write-through")
                memory[req.address / 4] = req.data;
            else
                block.state = "DIRTY";
        }
    }
};

class FullyAssociativeCache {
public:
    vector<CacheBlock> cache;
    string writePolicy;
    ReplacementPolicy policy;
    int ways;
    int blockSize;
    int count;
    int globalTime;

    FullyAssociativeCache(int assoc, string wp,int wordsPerBlock,ReplacementPolicy rp) {

        ways = assoc;
        writePolicy = wp;
        policy = rp;
        blockSize = wordsPerBlock;
        globalTime = 0;
        count = 0;

        cache = vector<CacheBlock>(ways, CacheBlock(blockSize));
    }

    int getTag(int addr) {
        return addr / (4 * blockSize);
    }

    int getOffset(int addr) {
        return (addr / 4) % blockSize;
    }

    int chooseVictim() {

        if (policy == RANDOM_POLICY)
            return rand() % ways;

        if (policy == LRU_POLICY) {
            int victim = 0;
            for (int i = 1; i < ways; i++)
                if (cache[i].lastUsed < cache[victim].lastUsed)
                    victim = i;
            return victim;
        }

        // LIFO
        int victim = 0;
        for (int i = 1; i < ways; i++)
            if (cache[i].insertTime > cache[victim].insertTime)
                victim = i;
        return victim;
    }

    void writeback(int idx, vector<int>& memory) {
        int base = cache[idx].tag * blockSize;
        for (int i = 0; i < blockSize; i++)
            memory[base + i] = cache[idx].data[i];
    }

    void loadBlock(int idx, int tag, vector<int>& memory) {
        int base = tag * blockSize;
        for (int i = 0; i < blockSize; i++)
            cache[idx].data[i] = memory[base + i];

        cache[idx].tag = tag;
        cache[idx].state = "V";
        cache[idx].insertTime = globalTime;
        cache[idx].lastUsed = globalTime;
    }

    void serviceRequest(CPUReq req, vector<int>& memory,
                        int& hit, int& miss) {

        globalTime++;

        int tag = getTag(req.address);
        int offset = getOffset(req.address);

        int blockIndex = -1;

        for (int i = 0; i < ways; i++)
            if (cache[i].state != "I" && cache[i].tag == tag) {
                blockIndex = i;
                break;
            }

        if (blockIndex != -1)
            hit++;
        else {
            miss++;

            if (count < ways)
                blockIndex = count++;
            else {
                blockIndex = chooseVictim();
                if (cache[blockIndex].state == "DIRTY")
                    writeback(blockIndex, memory);
            }

            loadBlock(blockIndex, tag, memory);
        }

        cache[blockIndex].lastUsed = globalTime;

        if (req.type == "WRITE") {
            cache[blockIndex].data[offset] = req.data;

            if (writePolicy == "write-through")
                memory[req.address / 4] = req.data;
            else
                cache[blockIndex].state = "DIRTY";
        }
    }
};

/* ---------------- SET ASSOCIATIVE CACHE ---------------- */

class SetAssociativeCache {
public:
    int numSets;
    int ways;
    int blockSize;
    string writePolicy;
    ReplacementPolicy policy;
    int globalTime;

    vector<vector<CacheBlock>> cache;
    vector<int> setCount;

    SetAssociativeCache(int sets, int assoc,
                        string wp, int wordsPerBlock,
                        ReplacementPolicy rp) {

        numSets = sets;
        ways = assoc;
        blockSize = wordsPerBlock;
        writePolicy = wp;
        policy = rp;
        globalTime = 0;

        cache.resize(numSets,
                     vector<CacheBlock>(ways,
                     CacheBlock(blockSize)));

        setCount.resize(numSets, 0);
    }

    int getIndex(int addr) {
        return (addr / (4 * blockSize)) % numSets;
    }

    int getTag(int addr) {
        return (addr / (4 * blockSize)) / numSets;
    }

    int getOffset(int addr) {
        return (addr / 4) % blockSize;
    }

    int chooseVictim(int setIdx) {

        if (policy == RANDOM_POLICY)
            return rand() % ways;

        if (policy == LRU_POLICY) {
            int victim = 0;
            for (int i = 1; i < ways; i++)
                if (cache[setIdx][i].lastUsed <
                    cache[setIdx][victim].lastUsed)
                    victim = i;
            return victim;
        }

        int victim = 0;
        for (int i = 1; i < ways; i++)
            if (cache[setIdx][i].insertTime >
                cache[setIdx][victim].insertTime)
                victim = i;
        return victim;
    }

    void writeback(int setIdx, int way,
                   vector<int>& memory) {

        int base =
            (cache[setIdx][way].tag * numSets
            + setIdx) * blockSize;

        for (int i = 0; i < blockSize; i++)
            memory[base + i] =
                cache[setIdx][way].data[i];
    }

    void loadBlock(int setIdx, int way,
                   int tag,
                   vector<int>& memory) {

        int base =
            (tag * numSets + setIdx)
            * blockSize;

        for (int i = 0; i < blockSize; i++)
            cache[setIdx][way].data[i] =
                memory[base + i];

        cache[setIdx][way].tag = tag;
        cache[setIdx][way].state = "V";
        cache[setIdx][way].insertTime = globalTime;
        cache[setIdx][way].lastUsed = globalTime;
    }

    void serviceRequest(CPUReq req,
                        vector<int>& memory,
                        int& hit,
                        int& miss) {

        globalTime++;

        int setIdx = getIndex(req.address);
        int tag = getTag(req.address);
        int offset = getOffset(req.address);

        int way = -1;

        for (int i = 0; i < ways; i++)
            if (cache[setIdx][i].state != "I" &&
                cache[setIdx][i].tag == tag) {
                way = i;
                break;
            }

        if (way != -1)
            hit++;
        else {
            miss++;

            if (setCount[setIdx] < ways)
                way = setCount[setIdx]++;
            else {
                way = chooseVictim(setIdx);

                if (cache[setIdx][way].state == "DIRTY")
                    writeback(setIdx, way, memory);
            }

            loadBlock(setIdx, way, tag, memory);
        }

        cache[setIdx][way].lastUsed = globalTime;

        if (req.type == "WRITE") {
            cache[setIdx][way].data[offset] =
                req.data;

            if (writePolicy == "write-through")
                memory[req.address / 4] = req.data;
            else
                cache[setIdx][way].state = "DIRTY";
        }
    }
};


int main() {

    srand(time(0));

    vector<int> memory(1024);
    for (int i = 0; i < 1024; i++)
        memory[i] = i * 10;

    int cacheSizeBytes, blockSizeBytes;
    int associativity;
    int policyChoice;
    string writePolicy;

    cout << "Cache Size (Bytes): ";
    cin >> cacheSizeBytes;

    cout << "Block Size (Bytes): ";
    cin >> blockSizeBytes;

    cout << "Associativity: ";
    cin >> associativity;

    cout << "Write Policy (write-back/write-through): ";
    cin >> writePolicy;

    cout << "Replacement (0=RANDOM,1=LRU,2=LIFO): ";
    cin >> policyChoice;

    ReplacementPolicy policy =
        (ReplacementPolicy)policyChoice;

    int wordsPerBlock = blockSizeBytes / 4;
    int numBlocks = cacheSizeBytes / blockSizeBytes;

    if (numBlocks == 0) {
        cout << "Invalid configuration: Cache too small.\n";
        return 0;
    }

    if (associativity > numBlocks) {
        cout << "Invalid associativity. It cannot exceed number of blocks.\n";
        return 0;
    }

    if (numBlocks % associativity != 0) {
        cout << "Associativity must divide number of blocks evenly.\n";
        return 0;
    }

    ifstream file("sample_input.txt");

    if (!file.is_open()) {
        cout << "Error opening sample_input.txt\n";
        return 0;
    }

    vector<CPUReq> requests;
    string type;
    int addr, data;

    while (file >> type >> addr >> data)
        requests.push_back({type, addr, data});

    file.close();

    int hit = 0, miss = 0;

    if (associativity == 1) {

        DirectMappedCache cache(numBlocks,writePolicy,wordsPerBlock);

        for (auto r : requests)
            cache.serviceRequest(r, memory,
                                 hit, miss);
    }
    else if (associativity == numBlocks) {

        FullyAssociativeCache cache(numBlocks,writePolicy,wordsPerBlock,policy);

        for (auto r : requests)
            cache.serviceRequest(r, memory,hit, miss);
    }
    else {

        int sets = numBlocks / associativity;

        SetAssociativeCache cache(sets,associativity,writePolicy,wordsPerBlock,policy);

        for (auto r : requests)
            cache.serviceRequest(r, memory, hit, miss);
    }

    cout << "\nHits: " << hit << endl;
    cout << "Misses: " << miss << endl;

    if (hit + miss == 0)
        return 0;

    float missRate =
        (float)miss / (hit + miss);

    float amat = 1 + missRate * 10;

    cout << "Miss Rate: " << missRate << endl;
    cout << "AMAT: " << amat << endl;

    return 0;
}