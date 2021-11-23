/********************************************************************************
 *    Copyright (C) 2020 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include "Header.h"

#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <string>
#include <iomanip>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace std;
using namespace example_n_m;
namespace bpo = boost::program_options;

struct TFBuffer
{
    FairMQParts parts;
    chrono::steady_clock::time_point start;
    chrono::steady_clock::time_point end;
};

struct Receiver : fair::mq::Device
{
    void InitTask() override
    {
        fNumSenders = GetConfig()->GetValue<int>("num-senders");
        fBufferTimeoutInMs = GetConfig()->GetValue<int>("buffer-timeout");
        fMaxBuffers = GetConfig()->GetValue<int>("max-buffers");
    }

    void Run() override
    {
        fair::mq::Channel& input = GetChannel("data", 0);
        fair::mq::Channel& output = GetChannel("sink", 0);

        while (!NewStatePending()) {
            FairMQParts parts;
            if (input.Receive(parts, 100) < 0) {
                DiscardIncompleteBuffers();
                continue;
            }

            Header& h = *(static_cast<Header*>(parts.At(0)->GetData()));
            // LOG(info) << "Received sub-buffer #" << h.id << " from Sender" << h.senderIndex;

            if (fDiscardedSet.find(h.id) == fDiscardedSet.end()) {
                if (fBuffers.find(h.id) == fBuffers.end()) {
                    // if this is the first part with this ID, save the receive time.
                    fBuffers[h.id].start = chrono::steady_clock::now();
                }
                // if the received ID has not previously been discarded, store the data part in the buffer
                fBuffers[h.id].parts.AddPart(move(parts.At(1)));
            } else {
                // if received ID has been previously discarded.
                LOG(debug) << "Received part from an already discarded timeframe with id " << h.id;
            }

            if (fBuffers[h.id].parts.Size() == fNumSenders) {
                // LOG(info) << "Successfully completed buffer #" << h.id << ", sending to sink";
                if (output.Send(fBuffers[h.id].parts) < 0) {
                    LOG(debug) << "could not send buffer to sink, discarding";
                }
                fBuffers.erase(h.id);

                if (fMaxBuffers > 0 && ++fTimeframeCounter >= fMaxBuffers) {
                    LOG(info) << "Reached configured maximum number of buffers (" << fMaxBuffers << "). Exiting RUNNING state.";
                    break;
                }
            }

            DiscardIncompleteBuffers();
        }
    }

    void DiscardIncompleteBuffers()
    {
        auto it = fBuffers.begin();
        while (it != fBuffers.end()) {
            if (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - (it->second).start).count() > fBufferTimeoutInMs) {
                LOG(debug) << "Timeframe #" << it->first << " incomplete after " << fBufferTimeoutInMs << " milliseconds, discarding";
                fDiscardedSet.insert(it->first);
                fBuffers.erase(it++);
                LOG(debug) << "Number of discarded buffers: " << fDiscardedSet.size();
            } else {
                // LOG(info) << "Timeframe #" << it->first << " within timeout, buffering...";
                ++it;
            }
        }
    }

    void ResetTask() override
    {

    }

  private:
    unordered_map<uint16_t, TFBuffer> fBuffers;
    unordered_set<uint16_t> fDiscardedSet;

    int fNumSenders = 0;
    int fBufferTimeoutInMs = 5000;
    int fMaxBuffers = 0;
    int fTimeframeCounter = 0;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
        ("buffer-timeout", bpo::value<int>()->default_value(1000), "Buffer timeout in milliseconds")
        ("num-senders", bpo::value<int>()->required(), "Number of senders")
        ("max-buffers", bpo::value<int>()->default_value(0), "Maximum number of buffers to receive (0 - unlimited)");
}

std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions& /* config */)
{
    return std::make_unique<Receiver>();
}
