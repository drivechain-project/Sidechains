// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIDECHAINCLIENT_H
#define SIDECHAINCLIENT_H

#include "primitives/sidechain.h"

#include <string>
#include <vector>

#include <boost/property_tree/json_parser.hpp>

class SidechainClient
{
public:
    SidechainClient();

    /*
     * Send B-WT^ hash to local node
     */
    bool BroadcastWTJoin(const std::string& hex);

    /*
     * Ask for an updated list of recent deposits
     */
    std::vector<SidechainDeposit> UpdateDeposits(uint8_t nSidechain);

    /*
     * Note: true return value indicates the request to verify the
     * txout proof went through. However, txid (returned by reference)
     * must also be verified to match the txCritical hash.
     */
    bool VerifyCriticalHashProof(const std::string& criticalProof, uint256& txid);

private:
    /*
     * Send json request to local node
     */
    bool SendRequestToMainchain(const std::string& json, boost::property_tree::ptree &ptree);
};

#endif // SIDECHAINCLIENT_H
