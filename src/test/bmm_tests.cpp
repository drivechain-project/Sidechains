// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/sigcache.h"
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
    // Check that valid sidechain critical hash commit is added to ratchet
    SidechainDB scdb;

    // Create coinbase with h* commit
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 486604799;

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

    // Add h* commit to coinbase
    mtx.vout.push_back(CTxOut(50 * CENT, GenerateCriticalHashCommitment(criticalData)));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was added
    BOOST_CHECK(scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_invalid_sidechain)
{
    // Commit with invalid sidechain number should be ignored
    SidechainDB scdb;

    // Create coinbase with h* commit
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 486604799;

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

    // Add h* commit to coinbase
    mtx.vout.push_back(CTxOut(50 * CENT, GenerateCriticalHashCommitment(criticalData)));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was rejected
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_invalid_dagnumber_limit)
{
    // An h* commit with a DAG number > BMM_MAX_DAG
    SidechainDB scdb;

    // Create coinbase with h* commit
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 486604799;

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    bytes << CScriptNum::serialize(0);
    bytes << CScriptNum::serialize(BMM_MAX_DAG + 1);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Add h* commit to coinbase
    mtx.vout.push_back(CTxOut(50 * CENT, GenerateCriticalHashCommitment(criticalData)));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was rejected from the ratchet
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_invalid_dagnumber)
{
    // Commit with invalid dag number should be ignored
    SidechainDB scdb;

    // Add some valid data to the ratchet

    // Create coinbase with h* commit
    CMutableTransaction mtx;
    mtx.nVersion = 1;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript() << 486604799;

    // Create critical data
    CScript bytes;
    bytes.resize(3);
    bytes[0] = 0x00;
    bytes[1] = 0xbf;
    bytes[2] = 0x00;

    bytes << CScriptNum::serialize(0);
    bytes << CScriptNum::serialize(21);

    CCriticalData criticalData;
    criticalData.bytes = std::vector<unsigned char>(bytes.begin(), bytes.end());
    criticalData.hashCritical = GetRandHash();

    // Add h* commit to coinbase
    mtx.vout.push_back(CTxOut(50 * CENT, GenerateCriticalHashCommitment(criticalData)));

    // Update SCDB so that h* is processed
    uint256 hashBlock = GetRandHash();
    std::string strError = "";
    scdb.Update(0, hashBlock, mtx.vout, strError);

    // Verify that h* was rejected from the ratchet
    BOOST_CHECK(!scdb.HaveLinkingData(SIDECHAIN_TEST, criticalData.hashCritical));
}

BOOST_AUTO_TEST_CASE(bmm_maturity)
{
    // Test maturity rules of sidechain h* critical data transactions
}

BOOST_AUTO_TEST_SUITE_END()
