// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "sidechain.h"
#include "uint256.h"
#include "utilstrencodings.h"

SidechainDB::SidechainDB()
{
    SCDB.resize(ARRAYLEN(ValidSidechains));
}

void SidechainDB::AddDeposits(const std::vector<CTransaction>& vtx)
{
    std::vector<SidechainDeposit> vDeposit;
    for (const CTransaction& tx : vtx) {
        // Create sidechain deposit objects from transaction outputs
        for (const CTxOut& out : tx.vout) {
            const CScript &scriptPubKey = out.scriptPubKey;

            // scriptPubKey must contain keyID
            if (scriptPubKey.size() < sizeof(uint160))
                continue;
            if (scriptPubKey.front() != OP_RETURN)
                continue;

            uint8_t nSidechain = (unsigned int)scriptPubKey[1];
            if (!SidechainNumberValid(nSidechain))
                continue;

            CScript::const_iterator pkey = scriptPubKey.begin() + 2;
            opcodetype opcode;
            std::vector<unsigned char> vch;
            if (!scriptPubKey.GetOp(pkey, opcode, vch))
                continue;
            if (vch.size() != sizeof(uint160))
                continue;

            CKeyID keyID = CKeyID(uint160(vch));
            if (keyID.IsNull())
                continue;

            SidechainDeposit deposit;
            deposit.tx = tx;
            deposit.keyID = keyID;
            deposit.nSidechain = nSidechain;

            vDeposit.push_back(deposit);
        }
    }

    // Add deposits to cache
    for (const SidechainDeposit& d : vDeposit) {
        if (!HaveDepositCached(d))
            vDepositCache.push_back(d);
    }
}

bool SidechainDB::AddWTJoin(uint8_t nSidechain, const CTransaction& tx)
{
    if (vWTJoinCache.size() >= SIDECHAIN_MAX_WT)
        return false;
    if (!SidechainNumberValid(nSidechain))
        return false;
    if (HaveWTJoinCached(tx.GetHash()))
        return false;

    const Sidechain& s = ValidSidechains[nSidechain];

    std::vector<SidechainWTJoinState> vWT;

    SidechainWTJoinState wt;
    wt.nSidechain = nSidechain;
    wt.nBlocksLeft = s.GetTau();
    wt.nWorkScore = 0;
    wt.wtxid = tx.GetHash();

    vWT.push_back(wt);

    if (UpdateSCDBIndex(vWT)) {
        vWTJoinCache.push_back(tx);
        return true;
    }
    return false;
}

bool SidechainDB::ApplyDefaultUpdate()
{
    if (!HasState())
        return true;

    // Decrement nBlocksLeft, nothing else changes
    for (const Sidechain& s : ValidSidechains) {
        SCDBIndex& index = SCDB[s.nSidechain];
        for (SidechainWTJoinState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }
    return true;
}

bool SidechainDB::CheckWorkScore(const uint8_t& nSidechain, const uint256& wtxid) const
{
    if (!SidechainNumberValid(nSidechain))
        return false;

    std::vector<SidechainWTJoinState> vState = GetState(nSidechain);
    for (const SidechainWTJoinState& state : vState) {
        if (state.wtxid == wtxid) {
            if (state.nWorkScore >= ValidSidechains[nSidechain].nMinWorkScore)
                return true;
            else
                return false;
        }
    }
    return false;
}

std::vector<SidechainDeposit> SidechainDB::GetDeposits(uint8_t nSidechain) const
{
    std::vector<SidechainDeposit> vSidechainDeposit;
    for (size_t i = 0; i < vDepositCache.size(); i++) {
        if (vDepositCache[i].nSidechain == nSidechain)
            vSidechainDeposit.push_back(vDepositCache[i]);
    }
    return vSidechainDeposit;
}

uint256 SidechainDB::GetHash() const
{
    // TODO add LD data to the tree
    std::vector<uint256> vLeaf;
    for (const Sidechain& s : ValidSidechains) {
        std::vector<SidechainWTJoinState> vState = GetState(s.nSidechain);
        for (const SidechainWTJoinState& state : vState) {
            vLeaf.push_back(state.GetHash());
        }
    }
    return ComputeMerkleRoot(vLeaf);
}

uint256 SidechainDB::GetHashBlockLastSeen() const
{
    return hashBlockLastSeen;
}

uint256 SidechainDB::GetHashIfUpdate(const std::vector<SidechainWTJoinState>& vNewScores) const
{
    SidechainDB scdbCopy = (*this);
    scdbCopy.UpdateSCDBIndex(vNewScores);

    return (scdbCopy.GetHash());
}

std::multimap<uint256, int> SidechainDB::GetLinkingData() const
{
    return mapBMMLD;
}

std::vector<SidechainWTJoinState> SidechainDB::GetState(uint8_t nSidechain) const
{
    if (!HasState() || !SidechainNumberValid(nSidechain))
        return std::vector<SidechainWTJoinState>();

    std::vector<SidechainWTJoinState> vState;
    for (const SidechainWTJoinState& member : SCDB[nSidechain].members) {
        if (!member.IsNull())
            vState.push_back(member);
    }
    return vState;
}

std::vector<CTransaction> SidechainDB::GetWTJoinCache() const
{
    return vWTJoinCache;
}

bool SidechainDB::HasState() const
{
    // Make sure that SCDB is actually initialized
    if (SCDB.size() != ARRAYLEN(ValidSidechains))
        return false;

    // Check if any SCDBIndex(s) are populated
    if (SCDB[SIDECHAIN_TEST].IsPopulated())
        return true;
    else
    if (SCDB[SIDECHAIN_HIVEMIND].IsPopulated())
        return true;
    else
    if (SCDB[SIDECHAIN_WIMBLE].IsPopulated())
        return true;

    return false;
}

bool SidechainDB::HaveDepositCached(const SidechainDeposit &deposit) const
{
    for (const SidechainDeposit& d : vDepositCache) {
        if (d == deposit)
            return true;
    }
    return false;
}

bool SidechainDB::HaveWTJoinCached(const uint256& wtxid) const
{
    for (const CTransaction& tx : vWTJoinCache) {
        if (tx.GetHash() == wtxid)
            return true;
    }
    return false;
}

void SidechainDB::Reset()
{
    // Clear out SCDB
    for (const Sidechain& s : ValidSidechains)
        SCDB[s.nSidechain].ClearMembers();

    // Clear out LD
    mapBMMLD.clear();
    std::queue<uint256> queueEmpty;
    std::swap(queueBMMLD, queueEmpty);

    // Clear out Deposit data
    vDepositCache.clear();

    // Clear out cached WT^(s)
    vWTJoinCache.clear();

    // Reset hashBlockLastSeen
    hashBlockLastSeen.SetNull();
}

std::string SidechainDB::ToString() const
{
    std::string str;
    str += "SidechainDB:\n";
    for (const Sidechain& s : ValidSidechains) {
        // Print sidechain name
        str += "Sidechain: " + s.GetSidechainName() + "\n";
        // Print sidechain WT^ workscore(s)
        std::vector<SidechainWTJoinState> vState = GetState(s.nSidechain);
        for (const SidechainWTJoinState& state : vState) {
            str += "WT^: " + state.wtxid.ToString() + "\n";
            str += "workscore: " + std::to_string(state.nWorkScore) + "\n";
        }
        str += "\n";
    }
    return str;
}

bool SidechainDB::Update(int nHeight, const uint256& hashBlock, const std::vector<CTxOut>& vout, std::string& strError)
{
    if (hashBlock.IsNull())
        return false;
    if (!vout.size())
        return false;

    // TODO skip if nHeight < drivechains activation block height

    // If a sidechain's tau period ended, reset WT^ verification status
    for (const Sidechain& s : ValidSidechains) {
        if (nHeight > 0 && (nHeight % s.GetTau()) == 0)
            SCDB[s.nSidechain].ClearMembers();
    }
    // TODO clear out cached WT^(s) that belong to the Sidechain
    // that was just reset.

    /*
     * Now we will look for data that is relevent to SCDB
     * in this block's coinbase.
     *
     * Scan for h* linking data and add it to the BMMLD
     * ratchet system.
     *
     * Scan for new WT^(s) and start tracking them.
     *
     * Scan for updated SCDB MT hash, and perform MT hash
     * based SCDB update.
     *
     * Update hashBlockLastSeen to reflect that we have
     * scanned this latest block.
     */

    // TODO use CScript::IsBribeHashCommit() function once
    // format is finalized.
    // Scan for h*(s)
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        // Must at least contain the h*
        if (scriptPubKey.size() < 32)
            continue;
        if (!scriptPubKey.IsUnspendable())
            continue;

        CScript::const_iterator pbn = scriptPubKey.begin() + 1;
        opcodetype opcode;
        std::vector<unsigned char> vchBN;
        if (!scriptPubKey.GetOp(pbn, opcode, vchBN))
            continue;
        if (vchBN.size() < 1 || vchBN.size() > 4)
            continue;

        CScriptNum nBlock(vchBN, true);

        CScript::const_iterator phash = scriptPubKey.begin() + vchBN.size() + 2;
        std::vector<unsigned char> vch;
        if (!scriptPubKey.GetOp(phash, opcode, vch))
            continue;
        if (vch.size() != sizeof(uint256))
            continue;

        uint256 hashCritical = uint256(vch);

        // Check block number
        bool fValid = false;
        if (queueBMMLD.size()) {
            // Compare block number with most recent h* block number
            uint256 hashMostRecent = queueBMMLD.back();
            std::multimap<uint256, int>::const_iterator it = mapBMMLD.find(hashMostRecent);
            if (it == mapBMMLD.end())
                return false;

            int nHeightMostRecent = it->second;

            if ((nBlock.getint() - nHeightMostRecent) <= 1)
                fValid = true;
        } else {
            // No previous h* to compare with
            fValid = true;
        }
        if (!fValid) {
            strError = "SidechainDB::Update: h* invalid";
            continue;
        }

        // Update BMM linking data
        // Add new linking data
        mapBMMLD.emplace(hashCritical, nBlock.getint());
        queueBMMLD.push(hashCritical);

        // Remove old linking data if we need to
        if (mapBMMLD.size() > SIDECHAIN_MAX_LD) {
            uint256 hashRemove = queueBMMLD.front();
            std::multimap<uint256, int>::const_iterator it = mapBMMLD.lower_bound(hashRemove);
            if (it->first == hashRemove) {
                mapBMMLD.erase(it);
                queueBMMLD.pop();
            }
        }
    }

    // Scan for new WT^(s) and start tracking them
    // TODO
    // SidechainDB::AddWTJoin
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsWTHashCommit()) {
            // Get WT^ hash from script
            CScript::const_iterator phash = scriptPubKey.begin() + 7;
            opcodetype opcode;
            std::vector<unsigned char> vchHash;
            if (!scriptPubKey.GetOp(phash, opcode, vchHash))
                continue;
            if (vchHash.size() != 32)
                continue;

            uint256 hashWT = uint256(vchHash);

            // Check sidechain number
            CScript::const_iterator pnsidechain = scriptPubKey.begin() + 39;
            std::vector<unsigned char> vchNS;
            if (!scriptPubKey.GetOp(pnsidechain, opcode, vchNS))
            if (vchNS.size() < 1 || vchNS.size() > 4)
                continue;

            CScriptNum nSidechain(vchNS, true);
            if (!SidechainNumberValid(nSidechain.getint()))
                continue;

            // Create WT object
            std::vector<SidechainWTJoinState> vWT;

            SidechainWTJoinState wt;
            wt.nSidechain = nSidechain.getint();
            wt.nBlocksLeft = ValidSidechains[nSidechain.getint()].GetTau();
            wt.nWorkScore = 0;
            wt.wtxid = hashWT;

            vWT.push_back(wt);

            // Add to SCDB
            bool fUpdated = UpdateSCDBIndex(vWT);
            // TODO handle !fUpdated
        }
    }

    // Scan for updated SCDB MT hash and try to update
    // workscore of WT^(s)
    // Note: h*(s) and new WT^(s) must be added to SCDB
    // before this can be done.
    // Note: Only one MT hash commit is allowed per coinbase
    std::vector<CScript> vMTHashScript;
    for (const CTxOut& out : vout) {
        const CScript& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.IsMTHashCommit())
            vMTHashScript.push_back(scriptPubKey);
    }

    if (vMTHashScript.size() == 1) {
        const CScript& scriptPubKey = vMTHashScript.front();

        // Get MT hash from script
        CScript::const_iterator phash = scriptPubKey.begin() + 6;
        opcodetype opcode;
        std::vector<unsigned char> vch;
        if (scriptPubKey.GetOp(phash, opcode, vch) && vch.size() == 32) {
            // Try and sync
            uint256 hashMerkleRoot = uint256(vch);
            bool fUpdated = UpdateSCDBMatchMT(hashMerkleRoot);
            // TODO handle !fUpdated
        }
    }

    // Update hashBLockLastSeen
    hashBlockLastSeen = hashBlock;

    return true;
}

bool SidechainDB::UpdateSCDBIndex(const std::vector<SidechainWTJoinState>& vNewScores)
{
    // First check that sidechain numbers are valid
    for (const SidechainWTJoinState& s : vNewScores) {
        if (!SidechainNumberValid(s.nSidechain))
            return false;
    }

    // Decrement nBlocksLeft of existing WT^(s)
    for (const Sidechain& s : ValidSidechains) {
        SCDBIndex& index = SCDB[s.nSidechain];
        for (SidechainWTJoinState wt : index.members) {
            // wt is a copy
            wt.nBlocksLeft--;
            index.InsertMember(wt);
        }
    }

    // Apply new work scores
    for (const SidechainWTJoinState& s : vNewScores) {
        SCDBIndex& index = SCDB[s.nSidechain];
        SidechainWTJoinState wt;
        if (index.GetMember(s.wtxid, wt)) {
            // Update an existing WT^
            // Check that new work score is valid
            if ((wt.nWorkScore == s.nWorkScore) ||
                    (s.nWorkScore == (wt.nWorkScore + 1)) ||
                    (s.nWorkScore == (wt.nWorkScore - 1)))
            {
                index.InsertMember(s);
            }
        }
        else
        if (!index.IsFull()) {
            // Add a new WT^
            if (!s.nWorkScore == 0)
                continue;
            if (s.nBlocksLeft != ValidSidechains[s.nSidechain].GetTau())
                continue;
            index.InsertMember(s);
        }
    }
    return true;
}

bool SidechainDB::UpdateSCDBMatchMT(const uint256& hashMerkleRoot)
{
    // First see if we are already synchronized
    if (GetHash() == hashMerkleRoot)
        return true;

    // TODO Try a few optimizations before generating all possible
    // updates and testing them.

    // Optimization 1: While there is only 1 sidechain, we only have
    // to iterate through a single set of possible updates.

    // Optimization 2: Upvote any WT^(s) which have reached the end
    // of their wait period and test. We can assume that a WT^ that
    // was added will want to be upvoted once it can be otherwise
    // nobody would have spent the money to add it.

    // Optimization 3: Upvote again the WT^(s) which were
    // updated N blocks ago (t-1, t-2, t-3 etc)

    // Now we must test every possible update. Note that this
    // could be skipped if the updates are broadcast between
    // nodes of the bitcoin network.

    // Collect possible updates
    std::list<std::vector<SidechainWTJoinState>> input;
    for (const Sidechain& sidechain : ValidSidechains) {
        // Generate possible new states for this sidechain
        std::vector<SidechainWTJoinState> vPossible;
        std::vector<SidechainWTJoinState> vState = GetState(sidechain.nSidechain);
        for (const SidechainWTJoinState& state : vState) {
            SidechainWTJoinState stateCopy = state;

            // Decrement nBlocksLeft
            if (stateCopy.nBlocksLeft > 0)
                stateCopy.nBlocksLeft--;

            // Add copy that abstains
            vPossible.push_back(stateCopy);

            // Add copy with +1 work score
            SidechainWTJoinState stateCopyUpvote = stateCopy;
            stateCopyUpvote.nWorkScore++;
            vPossible.push_back(stateCopyUpvote);

            // Add copy with -1 work score
            SidechainWTJoinState stateCopyDownvote = stateCopy;
            if (stateCopyDownvote.nWorkScore) {
                stateCopyDownvote.nWorkScore--;
                vPossible.push_back(stateCopyDownvote);
            }
        }
        if (vPossible.size())
            input.push_back(vPossible);
    }

    std::list<std::vector<SidechainWTJoinState>> output;
    CartesianProduct(input, output);

    for (const std::vector<SidechainWTJoinState>& vWT : output) {
        if (GetHashIfUpdate(vWT) == hashMerkleRoot) {
            UpdateSCDBIndex(vWT);
            return (GetHash() == hashMerkleRoot);
        }
    }

    return false;
}

void CartesianProduct(std::list<std::vector<SidechainWTJoinState>> input, std::list<std::vector<SidechainWTJoinState>>& product)
{
    if (input.empty())
        return;

    // Get the first vector of possible updates
    const std::vector<SidechainWTJoinState>& vWT = input.front();

    // We need a pair to find Cartesian product
    if (vWT.size() < 2)
        return;

    // base case
    if (++input.begin() == input.end()) {
        for (const SidechainWTJoinState& wt : vWT)
            product.push_back({wt});
        return;
    }

    // Recurse
    CartesianProduct(std::list<std::vector<SidechainWTJoinState>>(++input.begin(), input.end()), product);

    // For each element in the first update vector, make a copy
    // of the result and append the element to it.
    std::list<std::vector<SidechainWTJoinState>> copies;
    for (size_t x = 1; x < vWT.size(); x++) {
        std::list<std::vector<SidechainWTJoinState>> copy = product;
        for (auto& c : copy)
            c.push_back(vWT[x]);
        copies.splice(copies.end(), copy);
    }

    // Add first element of first update vector to the result
    for (auto& out : product) {
        out.push_back(vWT.front());
    }

    // Add the copies we created earlier to the result
    product.splice(product.end(), copies);

    return;
}
