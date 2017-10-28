// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// TODO cleanup includes
#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "miner.h"
#include "random.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "validation.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(transaction_criticaldata_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(criticaldata_serialization)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vout.resize(1);

    mtx.vin[0].prevout.SetNull();
    mtx.vin[0].scriptSig = CScript();

    CScript script;
    script << OP_RETURN;

    mtx.vout[0] = CTxOut(50 * CENT, script);

    mtx.criticalData.hashCritical = GetRandHash();

    // Get the transaction's serialization
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    mtx.Serialize(ss);

    // Deserialize
    CTransaction txDeserialized(deserialize, ss);

    // Check that CTransaction was properly deserialized
    BOOST_CHECK(txDeserialized.GetHash() == mtx.GetHash());
}

BOOST_AUTO_TEST_CASE(criticaldata_valid)
{
    // Test in block with a valid data & commit
    BOOST_CHECK(chainActive.Height() == 100);

    // Generate a block
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Checking that we can make blocks normally
    BOOST_CHECK(chainActive.Height() == 101);

    // create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 102;

    // Add critical data
    mtx.criticalData.hashCritical = GetRandHash();

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Create coinbase with critical data commit
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();

    coinbaseTx.vout.push_back(CTxOut(50 * CENT, GenerateCriticalHashCommitment(mtx.criticalData)));

    // Create a block with critical data commit
    std::vector<CMutableTransaction> vtx;
    vtx.push_back(coinbaseTx);
    vtx.push_back(mtx);

    CreateAndProcessBlock(vtx, GetScriptForRawPubKey(coinbaseKey.GetPubKey()), true);

    BOOST_CHECK(chainActive.Height() == 102);
}

BOOST_AUTO_TEST_CASE(criticaldata_invalid_locktime)
{
    // Test in block with a valid data & commit but invalid locktime
    BOOST_CHECK(chainActive.Height() == 100);

    // Generate a block
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Checking that we can make blocks normally
    BOOST_CHECK(chainActive.Height() == 101);

    // create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 2600;

    // Add critical data
    mtx.criticalData.hashCritical = GetRandHash();

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Create a block with critical data commit
    std::vector<CMutableTransaction> vout;
    vout.push_back(mtx);

    CreateAndProcessBlock(vout, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Block should have been rejected, blockheight should be unchanged
    BOOST_CHECK(chainActive.Height() == 101);
}

BOOST_AUTO_TEST_CASE(criticaldata_invalid_no_commit)
{
    // Test in block with a valid data but no commit
    BOOST_CHECK(chainActive.Height() == 100);

    // Generate a block
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Checking that we can make blocks normally
    BOOST_CHECK(chainActive.Height() == 101);

    // create transaction with critical data
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    mtx.vout.resize(1);
    mtx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vout[0].scriptPubKey = CScript() << OP_0;
    mtx.vout[0].nValue = 50 * CENT;

    // Set locktime to the block we would like critical data to be commited in
    mtx.nLockTime = 102;

    // Add critical data
    mtx.criticalData.hashCritical = GetRandHash();

    // Sign
    const CTransaction txToSign(mtx);
    std::vector<unsigned char> vchSig;
    uint256 hash = SignatureHash(GetScriptForRawPubKey(coinbaseKey.GetPubKey()), txToSign, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
    BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Create a block with the critical data commit
    std::vector<CMutableTransaction> vtx;
    vtx.push_back(mtx);

    CreateAndProcessBlock(vtx, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    // Block should have been rejected, blockheight should be unchanged
    BOOST_CHECK(chainActive.Height() == 101);
}

BOOST_AUTO_TEST_SUITE_END()
