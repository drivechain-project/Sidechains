#ifndef BITCOIN_BMM_H
#define BITCOIN_BMM_H

#include "uint256.h"

#include <map>

class CBlock;

class BMM
{
public:
    BMM();

    bool StoreBMMBlock(const CBlock& block);

    bool GetBMMBlock(const uint256& hashBlock, CBlock& block);

private:
    std::map<uint256, CBlock> mapBMMBlocks;
};

#endif // BITCOIN_BMM_H
