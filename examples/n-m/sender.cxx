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

using namespace std;
using namespace example_n_m;
namespace bpo = boost::program_options;

struct Sender : fair::mq::Device
{
    void InitTask() override
    {
        fIndex = GetConfig()->GetProperty<int>("sender-index");
        fBufferSize = GetConfig()->GetProperty<int>("buffer-size");
        fNumReceivers = GetConfig()->GetProperty<int>("num-receivers");
    }

    void Run() override
    {
        FairMQChannel& dataInChannel = GetChannel("sync", 0);

        while (!NewStatePending()) {
            Header h;
            FairMQMessagePtr id(NewMessage());
            if (dataInChannel.Receive(id) > 0) {
                h.id = *(static_cast<uint64_t*>(id->GetData()));
                h.senderIndex = fIndex;
            } else {
                continue;
            }

            try {
                FairMQParts parts;
                parts.AddPart(NewSimpleMessageFor("data", 0, h));
                parts.AddPart(NewMessageFor("data", 0, fBufferSize));

                uint64_t currentDataId = h.id;
                int direction = currentDataId % fNumReceivers;

                if (Send(parts, "data", direction, 0) < 0) {
                    LOG(debug) << "Failed to queue Buffer #" << currentDataId << " to Receiver[" << direction << "]";
                }
            } catch (fair::mq::MessageBadAlloc& mba) {
                LOG(debug) << "shared memory full, retrying";
            }
        }
    }

  private:
    int fNumReceivers = 0;
    unsigned int fIndex = 0;
    int fBufferSize = 10000;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
        ("sender-index", bpo::value<int>()->default_value(0), "Sender Index")
        ("buffer-size", bpo::value<int>()->default_value(1000), "Buffer size in bytes")
        ("num-receivers", bpo::value<int>()->required(), "Number of Receivers");
}
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions& /* config */)
{
    return std::make_unique<Sender>();
}
