/********************************************************************************
 * Copyright (C) 2014-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <boost/algorithm/string.hpp>   // join/split
#include <cstddef>                      // size_t
#include <fairlogger/Logger.h>
#include <fairmq/Channel.h>
#include <fairmq/Properties.h>
#include <fairmq/Tools.h>
#include <fairmq/Transports.h>
#include <random>
#include <regex>
#include <set>

namespace fair::mq {

using namespace std;

template<typename T>
T GetPropertyOrDefault(const Properties& m, const string& k, const T& ifNotFound)
{
    if (m.count(k)) {
        return boost::any_cast<T>(m.at(k));
    }
    return ifNotFound;
}

constexpr Transport Channel::DefaultTransportType;
constexpr const char* Channel::DefaultTransportName;
constexpr const char* Channel::DefaultName;
constexpr const char* Channel::DefaultType;
constexpr const char* Channel::DefaultMethod;
constexpr const char* Channel::DefaultAddress;
constexpr int Channel::DefaultSndBufSize;
constexpr int Channel::DefaultRcvBufSize;
constexpr int Channel::DefaultSndKernelSize;
constexpr int Channel::DefaultRcvKernelSize;
constexpr int Channel::DefaultSndTimeoutMs;
constexpr int Channel::DefaultRcvTimeoutMs;
constexpr int Channel::DefaultLinger;
constexpr int Channel::DefaultRateLogging;
constexpr int Channel::DefaultPortRangeMin;
constexpr int Channel::DefaultPortRangeMax;
constexpr bool Channel::DefaultAutoBind;

Channel::Channel()
    : Channel(DefaultName, DefaultType, DefaultMethod, DefaultAddress, nullptr)
{}

Channel::Channel(const string& name)
    : Channel(name, DefaultType, DefaultMethod, DefaultAddress, nullptr)
{}

Channel::Channel(const string& type, const string& method, const string& address)
    : Channel(DefaultName, type, method, address, nullptr)
{}

Channel::Channel(const string& name, const string& type, shared_ptr<TransportFactory> factory)
    : Channel(name, type, DefaultMethod, DefaultAddress, factory)
{}

Channel::Channel(string name, string type, string method, string address, shared_ptr<TransportFactory> factory)
    : fTransportFactory(factory)
    , fTransportType(factory ? factory->GetType() : DefaultTransportType)
    , fSocket(factory ? factory->CreateSocket(type, name) : nullptr)
    , fName(std::move(name))
    , fType(std::move(type))
    , fMethod(std::move(method))
    , fAddress(std::move(address))
    , fSndBufSize(DefaultSndBufSize)
    , fRcvBufSize(DefaultRcvBufSize)
    , fSndKernelSize(DefaultSndKernelSize)
    , fRcvKernelSize(DefaultRcvKernelSize)
    , fSndTimeoutMs(DefaultSndTimeoutMs)
    , fRcvTimeoutMs(DefaultRcvTimeoutMs)
    , fLinger(DefaultLinger)
    , fRateLogging(DefaultRateLogging)
    , fPortRangeMin(DefaultPortRangeMin)
    , fPortRangeMax(DefaultPortRangeMax)
    , fAutoBind(DefaultAutoBind)
    , fValid(false)
    , fMultipart(false)
{
    // LOG(warn) << "Constructing channel '" << fName << "'";
}

Channel::Channel(const string& name, int index, const Properties& properties)
    : Channel(tools::ToString(name, "[", index, "]"), "unspecified", "unspecified", "unspecified", nullptr)
{
    string prefix(tools::ToString("chans.", name, ".", index, "."));

    fType = GetPropertyOrDefault(properties, string(prefix + "type"), std::string(DefaultType));
    fMethod = GetPropertyOrDefault(properties, string(prefix + "method"), std::string(DefaultMethod));
    fAddress = GetPropertyOrDefault(properties, string(prefix + "address"), std::string(DefaultAddress));
    fTransportType = TransportType(GetPropertyOrDefault(properties, string(prefix + "transport"), std::string(DefaultTransportName)));
    fSndBufSize = GetPropertyOrDefault(properties, string(prefix + "sndBufSize"), DefaultSndBufSize);
    fRcvBufSize = GetPropertyOrDefault(properties, string(prefix + "rcvBufSize"), DefaultRcvBufSize);
    fSndKernelSize = GetPropertyOrDefault(properties, string(prefix + "sndKernelSize"), DefaultSndKernelSize);
    fRcvKernelSize = GetPropertyOrDefault(properties, string(prefix + "rcvKernelSize"), DefaultRcvKernelSize);
    fSndTimeoutMs = GetPropertyOrDefault(properties, string(prefix + "sndTimeoutMs"), DefaultSndTimeoutMs);
    fRcvTimeoutMs = GetPropertyOrDefault(properties, string(prefix + "rcvTimeoutMs"), DefaultRcvTimeoutMs);
    fLinger = GetPropertyOrDefault(properties, string(prefix + "linger"), DefaultLinger);
    fRateLogging = GetPropertyOrDefault(properties, string(prefix + "rateLogging"), DefaultRateLogging);
    fPortRangeMin = GetPropertyOrDefault(properties, string(prefix + "portRangeMin"), DefaultPortRangeMin);
    fPortRangeMax = GetPropertyOrDefault(properties, string(prefix + "portRangeMax"), DefaultPortRangeMax);
    fAutoBind = GetPropertyOrDefault(properties, string(prefix + "autoBind"), DefaultAutoBind);
}

Channel::Channel(const Channel& chan)
    : Channel(chan, chan.fName)
{}

Channel::Channel(const Channel& chan, string newName)
    : fTransportFactory(nullptr)
    , fTransportType(chan.fTransportType)
    , fSocket(nullptr)
    , fName(std::move(newName))
    , fType(chan.fType)
    , fMethod(chan.fMethod)
    , fAddress(chan.fAddress)
    , fSndBufSize(chan.fSndBufSize)
    , fRcvBufSize(chan.fRcvBufSize)
    , fSndKernelSize(chan.fSndKernelSize)
    , fRcvKernelSize(chan.fRcvKernelSize)
    , fSndTimeoutMs(chan.fSndTimeoutMs)
    , fRcvTimeoutMs(chan.fRcvTimeoutMs)
    , fLinger(chan.fLinger)
    , fRateLogging(chan.fRateLogging)
    , fPortRangeMin(chan.fPortRangeMin)
    , fPortRangeMax(chan.fPortRangeMax)
    , fAutoBind(chan.fAutoBind)
    , fValid(false)
    , fMultipart(chan.fMultipart)
{}

Channel& Channel::operator=(const Channel& chan)
{
    if (this == &chan) {
        return *this;
    }

    fTransportFactory = nullptr;
    fTransportType = chan.fTransportType;
    fSocket = nullptr;
    fName = chan.fName;
    fType = chan.fType;
    fMethod = chan.fMethod;
    fAddress = chan.fAddress;
    fSndBufSize = chan.fSndBufSize;
    fRcvBufSize = chan.fRcvBufSize;
    fSndKernelSize = chan.fSndKernelSize;
    fRcvKernelSize = chan.fRcvKernelSize;
    fSndTimeoutMs = chan.fSndTimeoutMs;
    fRcvTimeoutMs = chan.fRcvTimeoutMs;
    fLinger = chan.fLinger;
    fRateLogging = chan.fRateLogging;
    fPortRangeMin = chan.fPortRangeMin;
    fPortRangeMax = chan.fPortRangeMax;
    fAutoBind = chan.fAutoBind;
    fValid = false;
    fMultipart = chan.fMultipart;

    return *this;
}

bool Channel::Validate()
try {
    stringstream ss;
    ss << "Validating channel '" << fName << "'... ";

    if (fValid) {
        ss << "ALREADY VALID";
        LOG(debug) << ss.str();
        return true;
    }

    // validate channel name
    smatch m;
    if (regex_search(fName, m, regex("[^a-zA-Z0-9\\-_\\[\\]#]"))) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "channel name contains illegal character: '" << m.str(0) << "', allowed characters are: a-z, A-Z, 0-9, -, _, [, ], #";
        return false;
    }

    // validate socket type
    const set<string> socketTypes{ "sub", "pub", "pull", "push", "req", "rep", "xsub", "xpub", "dealer", "router", "pair" };
    if (socketTypes.find(fType) == socketTypes.end()) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "Invalid channel type: '" << fType << "'";
        throw ChannelConfigurationError(tools::ToString("Invalid channel type: '", fType, "'"));
    }

    // validate socket address
    if (fAddress == "unspecified" || fAddress.empty()) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(debug) << "invalid channel address: '" << fAddress << "'";
        return false;
    } else {
        vector<string> endpoints;
        boost::algorithm::split(endpoints, fAddress, boost::algorithm::is_any_of(";"));
        for (const auto& endpoint : endpoints) {
            string address;
            if (endpoint[0] == '@' || endpoint[0] == '+' || endpoint[0] == '>') {
                address = endpoint.substr(1);
            } else {
                // we don't have a method modifier, check if the default method is set
                const set<string> socketMethods{ "bind", "connect" };
                if (socketMethods.find(fMethod) == socketMethods.end()) {
                    ss << "INVALID";
                    LOG(debug) << ss.str();
                    LOG(error) << "Invalid endpoint connection method: '" << fMethod << "' for " << endpoint;
                    throw ChannelConfigurationError(tools::ToString("Invalid endpoint connection method: '", fMethod, "' for ", endpoint));
                }
                address = endpoint;
            }
            // check if address is a tcp or ipc address
            if (address.compare(0, 6, "tcp://") == 0) {
                // check if TCP address contains port delimiter
                string addressString = address.substr(6);
                if (addressString.find(':') == string::npos) {
                    ss << "INVALID";
                    LOG(debug) << ss.str();
                    LOG(error) << "invalid channel address: '" << address << "' (missing port?)";
                    return false;
                }
            } else if (address.compare(0, 6, "ipc://") == 0) {
                // check if IPC address is not empty
                string addressString = address.substr(6);
                if (addressString.empty()) {
                    ss << "INVALID";
                    LOG(debug) << ss.str();
                    LOG(error) << "invalid channel address: '" << address << "' (empty IPC address?)";
                    return false;
                }
            } else if (address.compare(0, 9, "inproc://") == 0) {
                // check if IPC address is not empty
                string addressString = address.substr(9);
                if (addressString.empty()) {
                    ss << "INVALID";
                    LOG(debug) << ss.str();
                    LOG(error) << "invalid channel address: '" << address << "' (empty inproc address?)";
                    return false;
                }
            } else if (address.compare(0, 8, "verbs://") == 0) {
                // check if IPC address is not empty
                string addressString = address.substr(8);
                if (addressString.empty()) {
                    ss << "INVALID";
                    LOG(debug) << ss.str();
                    LOG(error) << "invalid channel address: '" << address << "' (empty verbs address?)";
                    return false;
                }
            } else {
                // if neither TCP or IPC is specified, return invalid
                ss << "INVALID";
                LOG(debug) << ss.str();
                LOG(error) << "invalid channel address: '" << address << "' (missing protocol specifier?)";
                return false;
            }
        }
    }

    // validate socket buffer size for sending
    if (fSndBufSize < 0) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "invalid channel send buffer size (cannot be negative): '" << fSndBufSize << "'";
        throw ChannelConfigurationError(tools::ToString("invalid channel send buffer size (cannot be negative): '", fSndBufSize, "'"));
    }

    // validate socket buffer size for receiving
    if (fRcvBufSize < 0) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "invalid channel receive buffer size (cannot be negative): '" << fRcvBufSize << "'";
        throw ChannelConfigurationError(tools::ToString("invalid channel receive buffer size (cannot be negative): '", fRcvBufSize, "'"));
    }

    // validate socket kernel transmit size for sending
    if (fSndKernelSize < 0) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "invalid channel send kernel transmit size (cannot be negative): '" << fSndKernelSize << "'";
        throw ChannelConfigurationError(tools::ToString("invalid channel send kernel transmit size (cannot be negative): '", fSndKernelSize, "'"));
    }

    // validate socket kernel transmit size for receiving
    if (fRcvKernelSize < 0) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "invalid channel receive kernel transmit size (cannot be negative): '" << fRcvKernelSize << "'";
        throw ChannelConfigurationError(tools::ToString("invalid channel receive kernel transmit size (cannot be negative): '", fRcvKernelSize, "'"));
    }

    // validate socket rate logging interval
    if (fRateLogging < 0) {
        ss << "INVALID";
        LOG(debug) << ss.str();
        LOG(error) << "invalid socket rate logging interval (cannot be negative): '" << fRateLogging << "'";
        throw ChannelConfigurationError(tools::ToString("invalid socket rate logging interval (cannot be negative): '", fRateLogging, "'"));
    }

    fValid = true;
    ss << "VALID";
    LOG(debug) << ss.str();
    return true;
} catch (exception& e) {
    LOG(error) << "Exception caught in Channel::ValidateChannel: " << e.what();
    throw ChannelConfigurationError(tools::ToString(e.what()));
}

void Channel::Init()
{
    fSocket = fTransportFactory->CreateSocket(fType, fName);

    // set linger duration (how long socket should wait for outstanding transfers before shutdown)
    fSocket->SetLinger(fLinger);

    // set high water marks
    fSocket->SetSndBufSize(fSndBufSize);
    fSocket->SetRcvBufSize(fRcvBufSize);

    // set kernel transmit size (set it only if value is not the default value)
    if (fSndKernelSize != 0) {
        fSocket->SetSndKernelSize(fSndKernelSize);
    }
    if (fRcvKernelSize != 0) {
        fSocket->SetRcvKernelSize(fRcvKernelSize);
    }
}

bool Channel::ConnectEndpoint(const string& endpoint)
{
    return fSocket->Connect(endpoint);
}

bool Channel::BindEndpoint(string& endpoint)
{
    // try to bind to the configured port. If it fails, try random one (if AutoBind is on).
    if (fSocket->Bind(endpoint)) {
        return true;
    } else {
        // auto-bind only implemented for TCP
        size_t protocolPos = endpoint.find(':');
        string protocol = endpoint.substr(0, protocolPos);
        if (protocol != "tcp") {
            return false;
        }

        if (fAutoBind) {
            // number of attempts when choosing a random port
            int numAttempts = 0;
            int maxAttempts = 1000;

            // initialize random generator
            default_random_engine generator(chrono::system_clock::now().time_since_epoch().count());
            uniform_int_distribution<int> randomPort(fPortRangeMin, fPortRangeMax);

            do {
                LOG(debug) << "Could not bind to configured (TCP) port (" << endpoint << "), trying random port in range " << fPortRangeMin << "-" << fPortRangeMax;
                ++numAttempts;

                if (numAttempts > maxAttempts) {
                    LOG(error) << "could not bind to any (TCP) port in the given range after " << maxAttempts << " attempts";
                    return false;
                }

                size_t pos = endpoint.rfind(':');
                endpoint = endpoint.substr(0, pos + 1) + tools::ToString(static_cast<int>(randomPort(generator)));
            } while (!fSocket->Bind(endpoint));

            return true;
        } else {
            return false;
        }
    }
}

std::string Channel::GetTransportName() const { return TransportName(fTransportType); }

Transport Channel::GetTransportType() const { return fTransportType; }

void Channel::UpdateTransport(const std::string& transport) { fTransportType = TransportType(transport); Invalidate(); }

} // namespace fair::mq
