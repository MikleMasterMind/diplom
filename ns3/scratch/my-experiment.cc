#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/loopback-net-device.h"
#include "ns3/network-module.h"
#include "ns3/per-packet-load-balancer.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/tcp-bbr.h"
#include "ns3/user-constants.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PerPacketLoadBalancerExperiment");

// =======================
// RTT tracing
// =======================

static Ptr<OutputStreamWrapper> g_rttStream;
static bool g_firstRtt = true;

static void
RttTracer(std::string context, Time oldval, Time newval)
{
    if (g_firstRtt)
    {
        *g_rttStream->GetStream() << "#Time(s) Rtt(s)\n";
        g_firstRtt = false;
    }
    *g_rttStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << " "
                              << context << "\n";
}

// =======================
// Goodput tracing (sink - application layer)
// =======================
static Ptr<OutputStreamWrapper> g_goodputStream;
static uint64_t g_goodputLastTotalRx = 0;
static Time g_goodputLastTime = Seconds(0);
static Ptr<PacketSink> g_sink;

static void
GoodputSampler()
{
    Time now = Simulator::Now();
    uint64_t totalRx = g_sink->GetTotalRx();

    double interval = (now - g_goodputLastTime).GetSeconds();
    if (interval > 0)
    {
        uint64_t deltaBytes = totalRx - g_goodputLastTotalRx;
        double mbps = (deltaBytes * 8.0) / interval / 1e6;

        *g_goodputStream->GetStream() << now.GetSeconds() << " " << mbps << "\n";

        g_goodputLastTotalRx = totalRx;
        g_goodputLastTime = now;
    }

    Simulator::Schedule(MilliSeconds(100), &GoodputSampler);
}

// =======================
// Throughput tracing (network layer - all received data including dupAck)
// =======================
static Ptr<OutputStreamWrapper> g_throughputStream;
static uint64_t g_throughputTotalRxBytes = 0;
static Time g_throughputLastTime = Seconds(0);

static void
IpRxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    g_throughputTotalRxBytes += packet->GetSize();
}

static void
ThroughputSampler()
{
    Time now = Simulator::Now();

    double interval = (now - g_throughputLastTime).GetSeconds();
    if (interval > 0)
    {
        uint64_t deltaBytes = g_throughputTotalRxBytes;
        double mbps = (deltaBytes * 8.0) / interval / 1e6;

        *g_throughputStream->GetStream() << now.GetSeconds() << " " << mbps << "\n";

        g_throughputTotalRxBytes = 0;
        g_throughputLastTime = now;
    }

    Simulator::Schedule(MilliSeconds(100), &ThroughputSampler);
}

// =======================
// BBR RTprop tracing
// =======================
static Ptr<OutputStreamWrapper> g_rtpropStream;
static bool g_firstRtprop = true;
static Ptr<OutputStreamWrapper> g_rtpropRawStream;
static bool g_firstRtpropRaw = true;

// =======================
// BBR BtlBw tracing
// =======================
static Ptr<OutputStreamWrapper> g_btlBwStream;
static bool g_firstBtlBw = true;
static Ptr<OutputStreamWrapper> g_btlBwRawStream;
static bool g_firstBtlBwRaw = true;

static void
BbrSampler()
{
    // Get all BBR objects
    Config::MatchContainer matches = Config::LookupMatches(
        "/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionOps/$ns3::TcpBbr");

    for (uint32_t i = 0; i < matches.GetN(); ++i)
    {
        Ptr<TcpBbr> bbr = matches.Get(i)->GetObject<TcpBbr>();
        if (i == 0 && bbr)
        {
            // Get RTprop (MinRtt) and BtlBw
            Time rtprop = bbr->GetMinRtt();
            DataRate btlBw = bbr->GetMaxBwFilter().GetBest();

            // Write RTprop to separate file
            if (g_firstRtprop)
            {
                *g_rtpropStream->GetStream() << "#Time(s) RTprop(s)\n";
                g_firstRtprop = false;
            }
            *g_rtpropStream->GetStream()
                << Simulator::Now().GetSeconds() << " " << rtprop.GetSeconds() << "\n";

                    // Write BtlBw to separate file
            if (g_firstBtlBw)
            {
                *g_btlBwStream->GetStream() << "#Time(s) BtlBw(Mbps)\n";
                g_firstBtlBw = false;
            }
            *g_btlBwStream->GetStream()
                        << Simulator::Now().GetSeconds() << " " << (btlBw.GetBitRate() / 1e6) << "\n";
        }
    }

    // Schedule next sample
    Simulator::Schedule(MilliSeconds(100), &BbrSampler);
}

static void
RtpropRawTracer(std::string context, Time oldval, Time newval)
{
    if (g_firstRtpropRaw)
    {
        *g_rtpropRawStream->GetStream() << "#Time(s) RTpropRaw(s) Context\n";
        g_firstRtpropRaw = false;
    }
    *g_rtpropRawStream->GetStream() << Simulator::Now().GetSeconds() << " "
                                    << newval.GetSeconds() << " " << context << "\n";
}

static void
BtlBwRawTracer(std::string context, uint64_t oldval, uint64_t newval)
{
    if (g_firstBtlBwRaw)
    {
        *g_btlBwRawStream->GetStream() << "#Time(s) BtlBwRaw(Mbps) Context\n";
        g_firstBtlBwRaw = false;
    }
    *g_btlBwRawStream->GetStream() << Simulator::Now().GetSeconds() << " " << (newval / 1e6)
                                   << " " << context << "\n";
}

// =======================
// Helper: add service IP to loopback
// =======================
static void
AddServiceIpToLoopback(Ptr<Node> node, Ipv4Address serviceIp)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();

    uint32_t loopIf = UINT32_MAX;
    for (uint32_t i = 0; i < node->GetNDevices(); ++i)
    {
        Ptr<NetDevice> nd = node->GetDevice(i);
        if (DynamicCast<LoopbackNetDevice>(nd))
        {
            loopIf = ipv4->GetInterfaceForDevice(nd);
            break;
        }
    }

    if (loopIf == UINT32_MAX)
    {
        NS_FATAL_ERROR("Loopback interface not found");
    }

    ipv4->AddAddress(loopIf, Ipv4InterfaceAddress(serviceIp, Ipv4Mask("/32")));
    ipv4->SetUp(loopIf);
}

// =======================
// main
// =======================
int
main(int argc, char* argv[])
{
    // Logging (keep moderate)
    LogComponentEnable("PerPacketLoadBalancerExperiment", LOG_LEVEL_INFO);
    LogComponentEnable("PerPacketLoadBalancer", LOG_LEVEL_INFO);

    Time simulationTime = Seconds(60);
    uint32_t numPaths = 2;
    uint32_t numBadPaths = 1;

    // Network parameters with defaults from user-constants.h
    std::string goodDataRate = GOOD_DATA_RATE;
    std::string goodDelay = GOOD_DELAY;
    std::string badDataRate = BAD_DATA_RATE;
    std::string badDelay = BAD_DELAY;

    bool ackFeatureExperiment = false;
#ifdef TCP_SOCKET_BASE_USE_NEW_DUPACK_LOGIC
    ackFeatureExperiment = true;
#endif

    bool lossClassification = false;
#ifdef NS3_IDFEF_SACK_LOSS_CLASSIFICATION
    lossClassification = true;
#endif

    bool sackEnabled = TCP_SOCKET_BASE_SACK_ENABLED;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("numPaths", "Number of parallel paths", numPaths);
    cmd.AddValue("numBadPaths", "Number of bad paths starting from path 1", numBadPaths);
    cmd.AddValue("goodDataRate", "Data rate for good paths", goodDataRate);
    cmd.AddValue("goodDelay", "Delay for good paths", goodDelay);
    cmd.AddValue("badDataRate", "Data rate for bad paths", badDataRate);
    cmd.AddValue("badDelay", "Delay for bad paths", badDelay);
    cmd.AddValue("sackEnabled", "Enable or disable TCP SACK", sackEnabled);
    cmd.Parse(argc, argv);

    numBadPaths = std::min(numBadPaths, numPaths);
    const std::string queueSize = "1000000B";

    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue(queueSize));

    // Directories
    std::vector<std::string> resultPathParts = {"result/data"};
    if (lossClassification)
    {
        resultPathParts.push_back("lossClassification");
    }
    else if (ackFeatureExperiment)
    {
        resultPathParts.push_back("ackFeature");
    }
    else
    {
        resultPathParts.push_back("balancing");
    }
    resultPathParts.push_back(std::to_string((int)simulationTime.GetSeconds()) + "-simulationTime");
    resultPathParts.push_back(std::to_string(numPaths) + "-numPaths");
    resultPathParts.push_back(std::to_string(numBadPaths) + "-numBadPaths");
    resultPathParts.push_back(goodDataRate + "-goodDataRate" + goodDelay + "-goodDelay");
    if (numBadPaths > 0) {
        resultPathParts.push_back(badDataRate + "-badDataRate" + badDelay + "-badDelay");
    }
    resultPathParts.push_back(sackEnabled ? "sackEnabled" : "sackDisabled");

    std::ostringstream dirBuilder;
    for (const auto& part : resultPathParts)
    {
        dirBuilder << part << "/";
    }
    std::string dir = dirBuilder.str();
    system(("mkdir -p " + dir).c_str());

    // ==========================
    // TCP CONFIG BEFORE APPS
    // ==========================
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(sackEnabled));

    // Make segment sizes consistent
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1460));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));

    // ==========================
    // Nodes
    // ==========================
    NodeContainer clientNode;
    NodeContainer balancerNode;
    NodeContainer forwarderNode;
    NodeContainer serverNode;
    NodeContainer routerNodes;

    clientNode.Create(1);
    balancerNode.Create(1);
    forwarderNode.Create(1);
    serverNode.Create(1);
    routerNodes.Create(numPaths);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(clientNode);
    internet.Install(balancerNode);
    internet.Install(forwarderNode);
    internet.Install(serverNode);
    internet.Install(routerNodes);

    // ==========================
    // Links
    // ==========================
    PointToPointHelper p2p;

    NetDeviceContainer clientToBalancerDevice;
    std::vector<NetDeviceContainer> balancerToRouterDevices;
    std::vector<NetDeviceContainer> routerToForwarderDevices;
    NetDeviceContainer forwarderToServerDevice;

    // Client -> Balancer (fat)
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    clientToBalancerDevice = p2p.Install(clientNode.Get(0), balancerNode.Get(0));

    // Balancer -> Routers (paths)
    for (uint32_t i = 0; i < numPaths; i++)
    {
        if (i >= (numPaths - numBadPaths))
        {
            p2p.SetDeviceAttribute("DataRate", StringValue(badDataRate));
            p2p.SetChannelAttribute("Delay", StringValue(badDelay));
        }
        else
        {
            p2p.SetDeviceAttribute("DataRate", StringValue(goodDataRate));
            p2p.SetChannelAttribute("Delay", StringValue(goodDelay));
        }

        balancerToRouterDevices.push_back(p2p.Install(balancerNode.Get(0), routerNodes.Get(i)));
    }

    // Routers -> Forwarder (same)
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    for (uint32_t i = 0; i < numPaths; i++)
    {
        routerToForwarderDevices.push_back(p2p.Install(routerNodes.Get(i), forwarderNode.Get(0)));
    }

    // Forwarder -> Server (same)
    forwarderToServerDevice = p2p.Install(forwarderNode.Get(0), serverNode.Get(0));

    // ==========================
    // IP addressing
    // ==========================
    Ipv4AddressHelper ipv4;

    if (numPaths > 100)
    {
        NS_FATAL_ERROR("This experiment supports at most 100 parallel paths");
    }

    // Reserve a dedicated /24 for each point-to-point link inside 10.0.0.0/8.
    // This gives enough unique subnets for 1 client-balancer link + 2*numPaths path links.
    auto makeSubnetBase = [](uint32_t subnetIndex) {
        NS_ABORT_MSG_IF(subnetIndex >= (1u << 16), "Subnet index exceeds 10.0.0.0/8 plan capacity");

        std::ostringstream network;
        network << "10." << ((subnetIndex / 256) % 256) << "." << (subnetIndex % 256) << ".0";
        return network.str();
    };

    uint32_t subnetIndex = 0;

    // Client-Balancer: 10.0.0.0/24
    ipv4.SetBase(makeSubnetBase(subnetIndex++).c_str(), "255.255.255.0");
    Ipv4InterfaceContainer clientToBalancerInterface = ipv4.Assign(clientToBalancerDevice);

    // Balancer-Router i: unique /24 per path
    std::vector<Ipv4InterfaceContainer> balancerToRouterInterfaces;
    for (uint32_t i = 0; i < numPaths; i++)
    {
        ipv4.SetBase(makeSubnetBase(subnetIndex++).c_str(), "255.255.255.0");
        balancerToRouterInterfaces.push_back(ipv4.Assign(balancerToRouterDevices[i]));
    }

    // Router i - Forwarder: another unique /24 per path
    std::vector<Ipv4InterfaceContainer> routerToForwarderInterfaces;
    for (uint32_t i = 0; i < numPaths; i++)
    {
        ipv4.SetBase(makeSubnetBase(subnetIndex++).c_str(), "255.255.255.0");
        routerToForwarderInterfaces.push_back(ipv4.Assign(routerToForwarderDevices[i]));
    }

    // Forwarder - Server: dedicated /24
    ipv4.SetBase(makeSubnetBase(subnetIndex++).c_str(), "255.255.255.0");
    Ipv4InterfaceContainer forwarderToServerInterface = ipv4.Assign(forwarderToServerDevice);

    // Add service IP on server loopback from a dedicated subnet outside link ranges
    Ipv4Address serviceIp("10.255.255.1");
    AddServiceIpToLoopback(serverNode.Get(0), serviceIp);

    // ==========================
    // Routing
    // ==========================

    // 1) Balancer uses PerPacketLoadBalancer for FORWARDED traffic (RouteInput)
    Ptr<PerPacketLoadBalancer> balancerLb = CreateObject<PerPacketLoadBalancer>();
    Ptr<Ipv4> balancerIpv4 = balancerNode.Get(0)->GetObject<Ipv4>();
    balancerIpv4->SetAttribute("IpForward", BooleanValue(true));
    balancerIpv4->SetRoutingProtocol(balancerLb);

    // Add multiple host routes to service IP through different routers/interfaces
    for (uint32_t i = 0; i < numPaths; i++)
    {
        Ipv4Address routerGw = balancerToRouterInterfaces[i].GetAddress(1); // router side
        uint32_t outIf = i + 2; // (0 loopback, 1 client link, then path links)

        balancerLb->AddHostRouteTo(serviceIp, routerGw, outIf);
    }

    // 2) Routers: static routing (normal)
    Ipv4StaticRoutingHelper staticHelper;

    for (uint32_t i = 0; i < numPaths; i++)
    {
        Ptr<Ipv4> rIpv4 = routerNodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> rStatic = staticHelper.GetStaticRouting(rIpv4);

        // Route to service IP (hosted on server loopback) via forwarder interface address
        uint32_t ifToForwarder = rIpv4->GetInterfaceForDevice(routerToForwarderDevices[i].Get(0));
        Ipv4Address forwarderNextHop =
            routerToForwarderInterfaces[i].GetAddress(1); // forwarder address on that link
        rStatic->AddHostRouteTo(serviceIp, forwarderNextHop, ifToForwarder);

        // Route back to client via balancer on balancer-router link
        uint32_t ifToBalancer = rIpv4->GetInterfaceForDevice(balancerToRouterDevices[i].Get(1));
        Ipv4Address balancerNextHop =
            balancerToRouterInterfaces[i].GetAddress(0); // balancer address on that link
        rStatic->AddHostRouteTo(clientToBalancerInterface.GetAddress(0),
                                balancerNextHop,
                                ifToBalancer);
    }

    // 3) Forwarder: pure IP forwarding between routers and server
    Ptr<Ipv4> forwarderIpv4 = forwarderNode.Get(0)->GetObject<Ipv4>();
    forwarderIpv4->SetAttribute("IpForward", BooleanValue(true));
    Ptr<Ipv4StaticRouting> forwarderStatic = staticHelper.GetStaticRouting(forwarderIpv4);

    uint32_t ifToServer = forwarderIpv4->GetInterfaceForDevice(forwarderToServerDevice.Get(0));
    Ipv4Address serverNextHop = forwarderToServerInterface.GetAddress(1);
    forwarderStatic->AddHostRouteTo(serviceIp, serverNextHop, ifToServer);

    for (uint32_t i = 0; i < numPaths; i++)
    {
        uint32_t ifToRouter =
            forwarderIpv4->GetInterfaceForDevice(routerToForwarderDevices[i].Get(1));
        Ipv4Address routerNextHop =
            routerToForwarderInterfaces[i].GetAddress(0); // router address on that link
        forwarderStatic->AddHostRouteTo(clientToBalancerInterface.GetAddress(0),
                                        routerNextHop,
                                        ifToRouter);
    }

    // 4) Server: send ACKs back via forwarder
    Ptr<Ipv4> serverIpv4 = serverNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> serverStatic = staticHelper.GetStaticRouting(serverIpv4);
    uint32_t ifToForwarder = serverIpv4->GetInterfaceForDevice(forwarderToServerDevice.Get(1));
    Ipv4Address forwarderNextHop = forwarderToServerInterface.GetAddress(0);
    serverStatic->AddHostRouteTo(clientToBalancerInterface.GetAddress(0),
                                 forwarderNextHop,
                                 ifToForwarder);

    // 5) Client: static route to service IP via balancer
    Ptr<Ipv4> clientIpv4 = clientNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> cStatic = staticHelper.GetStaticRouting(clientIpv4);
    // Client interface to balancer is typically 1 (0 loopback), but safer:
    uint32_t ifToBalancer = clientIpv4->GetInterfaceForDevice(clientToBalancerDevice.Get(0));
    cStatic->AddHostRouteTo(serviceIp, clientToBalancerInterface.GetAddress(1), ifToBalancer);

    // ==========================
    // Applications
    // ==========================
    uint16_t serverPort = 5000;
    Address serverAddress(InetSocketAddress(serviceIp, serverPort));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(serverNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", serverAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSend.SetAttribute("SendSize", UintegerValue(1460));

    ApplicationContainer clientApp = bulkSend.Install(clientNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(simulationTime - Seconds(1.0));

    // Grab sink pointer for goodput
    g_sink = DynamicCast<PacketSink>(serverApp.Get(0));

    // ==========================
    // Tracing: RTT (wildcard)
    // ==========================
    AsciiTraceHelper ascii;
    g_rttStream = ascii.CreateFileStream((dir + "rtt.data").c_str());

    // Connect after sockets are likely created
    Simulator::Schedule(Seconds(1.2), []() {
        Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/RTT",
                        MakeCallback(&RttTracer));
    });

    // ==========================
    // Tracing: BBR RTprop
    // ==========================
    g_rtpropStream = ascii.CreateFileStream((dir + "rtprop.data").c_str());
    g_rtpropRawStream = ascii.CreateFileStream((dir + "rtprop_raw.data").c_str());

    // ==========================
    // Tracing: BBR BtlBw
    // ==========================
    g_btlBwStream = ascii.CreateFileStream((dir + "btlbw.data").c_str());

    // Start periodic sampling of BBR metrics
    Simulator::Schedule(Seconds(1.5), &BbrSampler);
    g_btlBwRawStream = ascii.CreateFileStream((dir + "btlbw_raw.data").c_str());

    // Connect BBR metric traces after sockets are likely created
    Simulator::Schedule(Seconds(1.2), []() {
        Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionOps/$ns3::TcpBbr/RtPropRaw",
                        MakeCallback(&RtpropRawTracer));
        Config::Connect("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionOps/$ns3::TcpBbr/BtlBwRaw",
                        MakeCallback(&BtlBwRawTracer));
    });

    // ==========================
    // Tracing: Goodput time series (application layer)
    // ==========================
    g_goodputStream = ascii.CreateFileStream((dir + "goodput.data").c_str());
    *g_goodputStream->GetStream() << "#Time(s) Goodput(Mbps)\n";
    Simulator::Schedule(Seconds(1.5), &GoodputSampler);

    // ==========================
    // Tracing: Throughput time series (network layer - all received data)
    // ==========================
    g_throughputStream = ascii.CreateFileStream((dir + "throughput.data").c_str());
    *g_throughputStream->GetStream() << "#Time(s) Throughput(Mbps)\n";

    // Connect IP layer receive trace to capture all packets (including ACKs, dupAcks, etc.)
    Config::Connect("/NodeList/" + std::to_string(serverNode.Get(0)->GetId()) +
                        "/$ns3::Ipv4L3Protocol/Rx",
                    MakeCallback(&IpRxTrace));

    Simulator::Schedule(Seconds(1.5), &ThroughputSampler);

    // ==========================
    // FlowMonitor
    // ==========================
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // ==========================
    // Run
    // ==========================
    Simulator::Stop(simulationTime);
    Simulator::Run();

    NS_LOG_INFO("Simulation finished.");

    // ==========================
    // Output result files and line counts
    // ==========================
    std::vector<std::string> resultFiles = {"rtt.data",
                                            "rtprop.data",
                                            "rtprop_raw.data",
                                            "btlbw.data",
                                            "btlbw_raw.data",
                                            "goodput.data",
                                            "throughput.data"};

    NS_LOG_INFO("=== Result Files Summary ===");
    for (const auto& filename : resultFiles)
    {
        std::string filepath = dir + filename;
        std::ifstream file(filepath);
        if (file.is_open())
        {
            int lineCount = 0;
            std::string line;
            while (std::getline(file, line))
            {
                lineCount++;
            }
            file.close();
            NS_LOG_INFO(filepath << " - " << lineCount << " lines");
        }
        else
        {
            NS_LOG_INFO(filepath << " - file not found or empty");
        }
    }
    NS_LOG_INFO("============================");

    Simulator::Destroy();
    return 0;
}
