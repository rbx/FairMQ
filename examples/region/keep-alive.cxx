/********************************************************************************
 *    Copyright (C) 2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
#include <fairmq/shmem/Common.h>
#include <fairmq/shmem/UnmanagedRegion.h>
#include <fairmq/shmem/Segment.h>
#include <fairmq/shmem/Monitor.h>

#include <fairmq/tools/Unique.h>

#include <fairlogger/Logger.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <csignal>

#include <chrono>
#include <map>
#include <string>
#include <thread>

using namespace std;
using namespace boost::program_options;

namespace
{
    volatile sig_atomic_t gStopping = 0;
}

void signalHandler(int /* signal */)
{
    gStopping = 1;
}

struct ShmManager
{
    ShmManager(uint64_t _shmId, const vector<string>& _segments, const vector<string>& _regions)
        : shmId(fair::mq::shmem::makeShmIdStr(_shmId))
    {
        for (const auto& s : _segments) {
            vector<string> segmentConf;
            boost::algorithm::split(segmentConf, s, boost::algorithm::is_any_of(","));
            if (segmentConf.size() != 2) {
                LOG(error) << "incorrect format for --segments. Expecting pairs of <id>,<size>.";
                fair::mq::shmem::Monitor::Cleanup(fair::mq::shmem::ShmId{shmId});
                throw runtime_error("incorrect format for --segments. Expecting pairs of <id>,<size>.");
            }
            uint16_t id = stoi(segmentConf.at(0));
            uint64_t size = stoull(segmentConf.at(1));
            auto ret = segments.emplace(id, fair::mq::shmem::Segment(shmId, id, size, fair::mq::shmem::rbTreeBestFit));
            fair::mq::shmem::Segment& segment = ret.first->second;
            LOG(info) << "Created segment " << id << " of size " << segment.GetSize() << ", starting at " << segment.GetData() << ". Locking...";
            segment.Lock();
            LOG(info) << "Done.";
            LOG(info) << "Zeroing...";
            segment.Zero();
            LOG(info) << "Done.";
        }

        for (const auto& r : _regions) {
            vector<string> regionConf;
            boost::algorithm::split(regionConf, r, boost::algorithm::is_any_of(","));
            if (regionConf.size() != 2) {
                LOG(error) << "incorrect format for --regions. Expecting pairs of <id>,<size>.";
                fair::mq::shmem::Monitor::Cleanup(fair::mq::shmem::ShmId{shmId});
                throw runtime_error("incorrect format for --regions. Expecting pairs of <id>,<size>.");
            }
            uint16_t id = stoi(regionConf.at(0));
            uint64_t size = stoull(regionConf.at(1));
            auto ret = regions.emplace(id, make_unique<fair::mq::shmem::UnmanagedRegion>(shmId, id, size));
            fair::mq::shmem::UnmanagedRegion& region = *(ret.first->second);
            LOG(info) << "Created unamanged region " << id << " of size " << region.GetSize() << ", starting at " << region.GetData() << ". Locking...";
            region.Lock();
            LOG(info) << "Done.";
            LOG(info) << "Zeroing...";
            region.Zero();
            LOG(info) << "Done.";
        }
    }

    void ResetContent()
    {
        fair::mq::shmem::Monitor::ResetContent(fair::mq::shmem::ShmId{shmId});
    }

    ~ShmManager()
    {
        // clean all segments, regions and any other shmem objects belonging to this shmId
        fair::mq::shmem::Monitor::Cleanup(fair::mq::shmem::ShmId{shmId});
    }

    std::string shmId;
    map<uint16_t, fair::mq::shmem::Segment> segments;
    map<uint16_t, unique_ptr<fair::mq::shmem::UnmanagedRegion>> regions;
};

int main(int argc, char** argv)
{
    fair::Logger::SetConsoleColor(true);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        uint64_t shmId = 0;
        vector<string> segments;
        vector<string> regions;

        options_description desc("Options");
        desc.add_options()
            ("shmid", value<uint64_t>(&shmId)->required(), "Shm id")
            ("segments", value<vector<string>>(&segments)->multitoken()->composing(), "Segments, as <id>,<size> <id>,<size> <id>,<size> ...")
            ("regions",  value<vector<string>>(&regions)->multitoken()->composing(), "Regions, as <id>,<size> <id>,<size> <id>,<size> ...")
            ("help,h", "Print help");

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            LOG(info) << "ShmManager" << "\n" << desc;
            return 0;
        }

        notify(vm);

        ShmManager shmManager(shmId, segments, regions);

        while (!gStopping) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        LOG(info) << "stopping.";
    } catch (exception& e) {
        LOG(error) << "Unhandled Exception reached the top of main: " << e.what() << ", application will now exit";
        return 2;
    }

    return 0;
}
