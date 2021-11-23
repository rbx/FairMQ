/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <string>

namespace bpo = boost::program_options;

struct Sink : fair::mq::Device
{
    Sink()
    {
        OnData("sink", &Sink::HandleData);
    }

    void InitTask() override
    {
        // Get the fMaxBuffers value from the command line options (via fConfig)
        fMaxBuffers = fConfig->GetProperty<uint64_t>("max-buffers");
    }

    bool HandleData(fair::mq::Parts& parts, int)
    {
        if (fMaxBuffers > 0 && ++fNumBuffers >= fMaxBuffers) {
            LOG(info) << "Configured maximum number of buffers reached. Leaving RUNNING state.";
            return false;
        }

        // return true if you want the handler to be called again (otherwise return false go to the
        // Ready state)
        return true;
    }

  private:
    uint64_t fMaxBuffers = 0;
    uint64_t fNumBuffers = 0;
};

void addCustomOptions(bpo::options_description& options)
{
    options.add_options()
        ("max-buffers", bpo::value<uint64_t>()->default_value(0), "Maximum number of buffers of Run/ConditionalRun/OnData (0 - infinite)");
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<Sink>();
}
