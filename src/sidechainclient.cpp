// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sidechainclient.h"

#include "core_io.h"
#include "utilstrencodings.h"
#include "util.h"

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>

using boost::asio::ip::tcp;

SidechainClient::SidechainClient()
{
}

bool SidechainClient::BroadcastWTJoin(const std::string& hex)
{
    // JSON for sending the WT^ to mainchain via HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"SidechainClient\", ");
    json.append("\"method\": \"receivesidechainwt\", \"params\": ");
    json.append("[\"");
    json.append(std::to_string(THIS_SIDECHAIN.nSidechain));
    json.append("\",\"");
    json.append(hex);
    json.append("\"] }");

    // TODO Read result, display to user
    boost::property_tree::ptree ptree;
    return SendRequestToMainchain(json, ptree);
}

std::vector<SidechainDeposit> SidechainClient::UpdateDeposits(uint8_t nSidechain)
{
    // List of deposits in sidechain format for DB
    std::vector<SidechainDeposit> incoming;

    // JSON for requesting sidechain deposits via mainchain HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"SidechainClient\", ");
    json.append("\"method\": \"listsidechaindeposits\", \"params\": ");
    json.append("[\"");
    json.append(std::to_string(nSidechain));
    json.append("\"");
    json.append("] }");

    // Try to request deposits from mainchain
    boost::property_tree::ptree ptree;
    if (!SendRequestToMainchain(json, ptree))
        return incoming; // TODO display error

    // Process deposits
    BOOST_FOREACH(boost::property_tree::ptree::value_type &value, ptree.get_child("result")) {
        // Looping through list of deposits
        SidechainDeposit deposit;
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, value.second.get_child("")) {
            // Looping through this deposit's members
            if (v.first == "nSidechain") {
                // Read sidechain number
                std::string data = v.second.data();
                if (!data.length())
                    continue;
                uint8_t nSidechain = std::stoi(data);
                if (nSidechain != THIS_SIDECHAIN.nSidechain)
                    continue;

                deposit.nSidechain = nSidechain;
            }
            else
            if (v.first == "dtx") {
                // Read deposit transaction hex
                std::string data = v.second.data();
                if (!data.length())
                    continue;
                CMutableTransaction dtx;
                if (!DecodeHexTx(dtx, data))
                    continue;

                deposit.dtx = dtx;
            }
            else
            if (v.first == "keyID") {
                // Read keyID
                std::string data = v.second.data();
                if (!data.length())
                    continue;

                deposit.keyID.SetHex(data);
            }
        }

        // Verify that the deposit script represented exits in the tx
        bool depositValid = false;
        for (size_t i = 0; i < deposit.dtx.vout.size(); i++) {
            CScript scriptPubKey = deposit.dtx.vout[i].scriptPubKey;
            if (scriptPubKey.size() > 2 && scriptPubKey.IsWorkScoreScript()) {
                // Check sidechain number
                uint8_t nSidechain = (unsigned int)*scriptPubKey.begin();
                if (nSidechain != THIS_SIDECHAIN.nSidechain)
                    continue;
                if (nSidechain != deposit.nSidechain)
                    continue;

                CScript::const_iterator pkey = scriptPubKey.begin() + 1;
                std::vector<unsigned char> vch;
                opcodetype opcode;
                if (!scriptPubKey.GetOp2(pkey, opcode, &vch))
                    continue;
                if (vch.size() != sizeof(uint160))
                    continue;

                CKeyID keyID = CKeyID(uint160(vch));
                if (keyID.IsNull())
                    continue;
                if (keyID != deposit.keyID)
                    continue;

                depositValid = true;
            }
        }
        // Add this deposit to the list
        if (depositValid)
            incoming.push_back(deposit);
    }

    // return valid deposits in sidechain format
    return incoming;
}

bool SidechainClient::SendRequestToMainchain(const std::string& json, boost::property_tree::ptree &ptree)
{
    try {
        // Setup BOOST ASIO for a synchronus call to mainchain
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        tcp::resolver::query query("127.0.0.1", "18332");
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::resolver::iterator end;

        tcp::socket socket(io_service);
        boost::system::error_code error = boost::asio::error::host_not_found;

        // Try to connect
        while (error && endpoint_iterator != end)
        {
          socket.close();
          socket.connect(*endpoint_iterator++, error);
        }

        if (error) throw boost::system::system_error(error);

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "POST / HTTP/1.1\n";
        os << "Host: 127.0.0.1\n";
        os << "Content-Type: application/json\n";

        // Format user:pass for authentication
        std::string user = "";
        GetArg("-rpcuser", user);
        std::string pass = "";
        GetArg("-rpcpassword", pass);

        std::string auth = user + ":" + pass;
        if (auth == ":")
            return false;

        os << "Authorization: Basic " << EncodeBase64(auth) << std::endl;
        os << "Connection: close\n";
        os << "Content-Length: " << json.size() << "\n\n";
        os << json;

        // Send the request
        boost::asio::write(socket, output);

        // Read the reponse
        std::string data;
        for (;;)
        {
            boost::array<char, 4096> buf;

            // Read until end of file (socket closed)
            boost::system::error_code e;
            size_t sz = socket.read_some(boost::asio::buffer(buf), e);

            data.insert(data.size(), buf.data(), sz);

            if (e == boost::asio::error::eof)
                break; // socket closed
            else if (e)
                throw boost::system::system_error(e);
        }

        std::stringstream ss;
        ss << data;

        // Get response code
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        int code;
        ss >> code;

        // Check response code
        if (code != 200)
            return false;

        // Skip the rest of the header
        for (size_t i = 0; i < 5; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\r');

        // Parse json response;
        // TODO consider using univalue read_json instead of boost
        std::string JSON;
        ss >> JSON;
        std::stringstream jss;
        jss << JSON;
        boost::property_tree::json_parser::read_json(jss, ptree);
    } catch (std::exception &exception) {
        LogPrintf("ERROR Sidechain client (sendRequestToMainchain): %s\n", exception.what());
        return false;
    }
    return true;
}
