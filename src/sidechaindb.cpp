// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechaindb.h"

#include "chain.h"
#include "core_io.h"
#include "primitives/transaction.h"
#include "sidechainclient.h"
#include "utilstrencodings.h"

#include <iostream>
#include <sstream>

SidechainDB::SidechainDB()
{
}

bool SidechainWT::operator==(const SidechainWT& a) const
{
    return (a.nSidechain == nSidechain &&
            a.keyID == keyID &&
            a.wt == wt);
}

std::vector<SidechainWT> SidechainDB::GetWTCache() const
{
    return vWTCache;
}

std::vector<CTransaction> SidechainDB::GetWTJoinCache()
{
    return vWTJoinCache;
}

bool SidechainDB::HaveWTJoin(const uint256& txid) const
{
    for (const CTransaction& tx : vWTJoinCache) {
        if (txid == tx.GetHash())
            return true;
    }
    return false;
}

bool SidechainDB::HaveWT(const SidechainWT& wt) const
{
    for (const SidechainWT& w : vWTCache) {
        if (w == wt)
            return true;
    }
    return false;
}

std::string SidechainDB::ToString() const
{
    std::stringstream ss;
    return ss.str();
}

bool SidechainDB::AddSidechainWTJoin(const CTransaction& tx)
{
    // Check the WT^
    if (tx.IsNull())
        return false;
    if (vWTJoinCache.size() > SIDECHAIN_MAX_WT)
        return false;
    if (HaveWTJoin(tx.GetHash()))
        return false;
    // Try to send WT^ to the mainchain
    SidechainClient client;
    if (!client.BroadcastWTJoin(EncodeHexTx(tx)))
        return false;

    // Cache WT^
    vWTJoinCache.push_back(tx);
    return true;
}

bool SidechainDB::AddSidechainWT(const CTransaction &tx)
{
    for (size_t i = 0; i < tx.vout.size(); i++) {
        CScript script = tx.vout[i].scriptPubKey;
        if (script.size() > 2 && script.IsWTScript()) {
            std::vector<unsigned char> vch;
            opcodetype opcode;
            CScript::const_iterator pkey = script.begin() + 1;
            if (!script.GetOp2(pkey, opcode, &vch))
                return false;

            CKeyID keyID;
            keyID.SetHex(std::string(vch.begin(), vch.end()));

            if (keyID.IsNull())
                return false;

            SidechainWT wt;
            wt.nSidechain = THIS_SIDECHAIN.nSidechain;
            wt.keyID = keyID;
            wt.wt = tx;

            if (!HaveWT(wt))
                vWTCache.push_back(wt);
        }
    }
    return true;
}

std::string Sidechain::GetSidechainName() const
{
    return "SIDECHAIN_TEST";
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nWaitPeriod=" << nWaitPeriod << std::endl;
    ss << "nVerificationPeriod=" << nVerificationPeriod << std::endl;
    ss << "nMinWorkScore=" << nMinWorkScore << std::endl;
    ss << "depositScript=" << ScriptToAsmStr(depositScript) << std::endl;
    return ss.str();
}

std::string SidechainWT::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "keyID=" << keyID.ToString() << std::endl;
    ss << "wt=" << CTransaction(wt).ToString() << std::endl;
    return ss.str();
}
