#include "bmm.h"

#include "primitives/block.h"

BMM::BMM()
{

}

bool BMM::StoreBMMBlock(const CBlock& block)
{
    if (!block.vtx.size())
        return false;

    uint256 hashBlock = block.GetHash();

    // Already have block stored
    if (mapBMMBlocks.find(hashBlock) != mapBMMBlocks.end())
        return false;

    mapBMMBlocks[hashBlock] = block;

    return true;
}

bool BMM::GetBMMBlock(const uint256& hashBlock, CBlock& block)
{
    if (mapBMMBlocks.find(hashBlock) == mapBMMBlocks.end())
        return false;

    block = mapBMMBlocks[hashBlock];

    return true;
}
