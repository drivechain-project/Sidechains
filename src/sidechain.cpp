// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechain.h"

#include "hash.h"
#include "utilstrencodings.h"

#include <sstream>

std::string GetSidechainName(uint8_t nSidechain)
{
    if (!IsSidechainNumberValid(nSidechain))
        return "SIDECHAIN_UNKNOWN";

    return ValidSidechains[nSidechain].GetSidechainName();
}

std::string Sidechain::GetSidechainName() const
{
    // Check that number corresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
        return "SIDECHAIN_TEST";
    case SIDECHAIN_HIVEMIND:
        return "SIDECHAIN_HIVEMIND";
    case SIDECHAIN_WIMBLE:
        return "SIDECHAIN_WIMBLE";
    default:
        break;
    }
    return "SIDECHAIN_UNKNOWN";
}

uint16_t Sidechain::GetTau() const
{
    return nWaitPeriod + nVerificationPeriod;
}

int Sidechain::GetLastTauHeight(int nHeight) const
{
    uint16_t nTau = GetTau();
    for (;;) {
        if (nHeight < 0)
            return -1;
        if (nHeight % nTau == 0 || nHeight == 0)
            break;
        nHeight--;
    }
    return nHeight;
}

bool Sidechain::operator==(const Sidechain& a) const
{
    return (a.nSidechain == nSidechain);
}

std::string Sidechain::ToString() const
{
    std::stringstream ss;
    ss << "nSidechain=" << (unsigned int)nSidechain << std::endl;
    ss << "nWaitPeriod=" << nWaitPeriod << std::endl;
    ss << "nVerificationPeriod=" << nVerificationPeriod << std::endl;
    ss << "nMinWorkScore=" << nMinWorkScore << std::endl;
    return ss.str();
}

bool SidechainDeposit::operator==(const SidechainDeposit& a) const
{
    return (a.nSidechain == nSidechain &&
            a.keyID == keyID &&
            a.tx == tx);
}

std::string SidechainDeposit::ToString() const
{
    std::stringstream ss;
    ss << "sidechain=" << GetSidechainName(nSidechain) << std::endl;
    ss << "keyID=" << keyID.ToString() << std::endl;
    ss << "hashWTPrime=" << tx.GetHash().ToString() << std::endl;
    return ss.str();
}

bool SidechainLD::operator==(const SidechainLD& a) const
{
    return (a.nSidechain == nSidechain &&
            a.nPrevBlockRef == nPrevBlockRef &&
            a.hashCritical == hashCritical);
}

bool SidechainWTPrimeState::IsNull() const
{
    return (hashWTPrime.IsNull());
}

uint256 SidechainWTPrimeState::GetHash(void) const
{
    return SerializeHash(*this);
}

bool SidechainWTPrimeState::operator==(const SidechainWTPrimeState& a) const
{
    return (a.nSidechain == nSidechain &&
            a.hashWTPrime == hashWTPrime);
}

std::string SidechainWTPrimeState::ToString() const
{
    std::stringstream ss;
    ss << "hash=" << GetHash().ToString() << std::endl;
    ss << "sidechain=" << GetSidechainName(nSidechain) << std::endl;
    ss << "nBlocksLeft=" << (unsigned int)nBlocksLeft << std::endl;
    ss << "nWorkScore=" << (unsigned int)nWorkScore << std::endl;
    ss << "hashWTPrime=" << hashWTPrime.ToString() << std::endl;
    return ss.str();
}

bool SCDBIndex::IsPopulated() const
{
    // Do the least amount of work to determine whether
    // SCDBIndex is tracking anything. As the first slot
    // is populated first, we can just check if it is null.
    if (!members.front().IsNull())
        return true;

    return false;
}

bool SCDBIndex::IsFull() const
{
    for (const SidechainWTPrimeState& member : members) {
        if (member.IsNull())
            return false;
    }
    return true;
}

bool SCDBIndex::InsertMember(const SidechainWTPrimeState& member)
{
    for (size_t i = 0; i < members.size(); i++) {
        if (members[i].IsNull() || members[i].hashWTPrime == member.hashWTPrime) {
            members[i] = member;
            return true;
        }
    }
    return false;
}

void SCDBIndex::ClearMembers()
{
    for (size_t i = 0; i < members.size(); i++)
        members[i].hashWTPrime = uint256();
}

unsigned int SCDBIndex::CountPopulatedMembers() const
{
    unsigned int nMembers = 0;
    for (const SidechainWTPrimeState& member : members) {
        if (!member.IsNull())
            nMembers++;
    }
    return nMembers;
}

bool SCDBIndex::Contains(uint256 hashWT) const
{
    for (const SidechainWTPrimeState& member : members) {
        if (!member.IsNull() && member.hashWTPrime == hashWT)
            return true;
    }
    return false;
}

bool SCDBIndex::GetMember(uint256 hashWT, SidechainWTPrimeState& wt) const
{
    for (const SidechainWTPrimeState& member : members) {
        if (!member.IsNull() && member.hashWTPrime == hashWT) {
            wt = member;
            return true;
        }
    }
    return false;
}

bool IsSidechainNumberValid(uint8_t nSidechain)
{
    if (!(nSidechain < ValidSidechains.size()))
        return false;

    // Check that number corresponds to a valid sidechain
    switch (nSidechain) {
    case SIDECHAIN_TEST:
    case SIDECHAIN_HIVEMIND:
    case SIDECHAIN_WIMBLE:
        return true;
    default:
        return false;
    }
}
