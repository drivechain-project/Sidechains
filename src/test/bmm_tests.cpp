// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "sidechain.h"
#include "sidechaindb.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bmm_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(bmm_valid)
{
    /*
    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    bytes << CScriptNum::serialize(SIDECHAIN_TEST);
    bytes << CScriptNum::serialize(0);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 0;

    // Add critical data
    mtx.criticalData = criticalData;

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Create dummy coinbase
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 486604799;

    // Add dummy coinbase & critical data tx to block
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    // Generate commit
    GenerateCriticalHashCommitment(block, Params().GetConsensus());

    // Copy coinbase from block
    CMutableTransaction commit(*block.vtx[0]);

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, commit.vout, strError);

    // Verify that h* was added
    BOOST_CHECK(scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));

    // Reset SCDB after testing
    scdb.Reset();
    */
}

BOOST_AUTO_TEST_CASE(bmm_invalid_sidechain)
{
    // Commit with invalid sidechain number should be ignored
    SidechainDB scdb;

    // Create dummy coinbase
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 486604799;

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    // Use invalid sidechain number
    bytes << CScriptNum::serialize(2600);
    bytes << CScriptNum::serialize(0);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 101;

    // Add critical data
    mtx.criticalData = criticalData;

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Add dummy coinbase & critical data tx to block
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    // Generate commit
    GenerateCriticalHashCommitment(block, Params().GetConsensus());

    // Copy coinbase from block
    CMutableTransaction commit(*block.vtx[0]);

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, commit.vout, strError);

    // Verify that h* was rejected
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_invalid_prevblockref_limit)
{
    // Commit with invalid nPrevBlockRef (greater than limit) should be ignored
    SidechainDB scdb;

    // Create dummy coinbase
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 486604799;

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    // Use invalid nPrevBlockRef > BMM_MAX_PREVBLOCK
    bytes << CScriptNum::serialize(0);
    bytes << CScriptNum::serialize(BMM_MAX_PREVBLOCK + 1);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 101;

    // Add critical data
    mtx.criticalData = criticalData;

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Add dummy coinbase & critical data tx to block
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    // Generate commit
    GenerateCriticalHashCommitment(block, Params().GetConsensus());

    // Copy coinbase from block
    CMutableTransaction commit(*block.vtx[0]);

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, commit.vout, strError);

    // Verify that h* was rejected
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_invalid_prevblockref)
{
    // Commit with invalid nPrevBlockRef (greater than limit) should be ignored
    SidechainDB scdb;

    // Create dummy coinbase
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vin[0].scriptSig = CScript() << 486604799;

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    // Use invalid nPrevBlockRef > BMM_MAX_PREVBLOCK
    bytes << CScriptNum::serialize(0);
    bytes << CScriptNum::serialize(21);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 101;

    // Add critical data
    mtx.criticalData = criticalData;

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Add dummy coinbase & critical data tx to block
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.vtx.push_back(MakeTransactionRef(std::move(mtx)));

    // Generate commit
    GenerateCriticalHashCommitment(block, Params().GetConsensus());

    // Copy coinbase from block
    CMutableTransaction commit(*block.vtx[0]);

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, commit.vout, strError);

    // Verify that h* was rejected
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_maturity)
{
    // Test maturity rules of sidechain h* critical data transactions
}

BOOST_AUTO_TEST_SUITE_END()
