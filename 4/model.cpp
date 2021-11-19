#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <fstream>
#include <string>
#include <iostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

#define CSMA_NUM 90
#define LIV_TIME 10.0
#define CH_DELAY 300
#define EXPONENT 100.0
#define PCK_SIZE 1500

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("App");

using generator = std::default_random_engine;
using distribution = std::exponential_distribution<double>;

uint64_t pack_sent_all = 0;
uint64_t pack_droped_all = 0;
uint64_t queue_sum[CSMA_NUM];
uint64_t pack_send[CSMA_NUM];
uint64_t max_queue = 0;
uint64_t all_backofs = 0;

class App : public Application {
    virtual void StartApplication();
    virtual void StopApplication();

    void shed_next();
    void send_pack();

    Ptr<Socket> sock;
    Address addr;
    uint32_t pack_sz;
    EventId send_event;
    bool run;
    uint32_t packets_sent;
    generator *gen;
    distribution *distr;
    Ptr<Queue<Packet>> que;
    std::string name;
    int m_num;
public:
    App();
    virtual ~App();
    void Init(Ptr<Socket> sock_,
              Address addr_,
              uint32_t pack_sz_,
              generator *gen_,
              distribution *distr_,
              Ptr<Queue<Packet>> que_,
              std::string name_,
              int num_);
};

void App::StartApplication()
{
    run = true;
    packets_sent = 0;
    sock->Bind();
    sock->Connect(addr);
    sock->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    sock->SetAllowBroadcast(true);
    send_event = Simulator::Schedule(Seconds(0.0), &App::send_pack, this);
}

void App::StopApplication()
{
    run = false;
    if(send_event.IsRunning())
        Simulator::Cancel(send_event);
    if(sock)
        sock->Close();
}

void App::send_pack()
{
    Ptr<Packet> packet = Create<Packet>(pack_sz);
    sock->Send(packet);
    pack_sent_all++;

    shed_next();
}

void App::shed_next()
{
    if(run) {
        Time Next(MilliSeconds(1000 * (*distr)(*gen)));
        queue_sum[m_num] += que->GetNPackets();
        pack_send[m_num]++;
        if(que->GetNPackets() > max_queue)
            max_queue = que->GetNPackets();
        send_event = Simulator::Schedule(Next, &App::send_pack, this);
    }
}

App::App(): sock(0),
    addr(),
    pack_sz(0),
    send_event(),
    run(false),
    packets_sent(0)
{}

App::~App()
{
    sock = 0;
    delete gen;
    delete distr;
}

void App::Init(Ptr<Socket> sock_,
    Address addr_,
    uint32_t pack_sz_,
    generator *gen_,
    distribution *distr_,
    Ptr<Queue<Packet>> que_,
    std::string name_,
    int num_)
{
    sock = sock_;
    addr = addr_;
    pack_sz = pack_sz_;
    gen = gen_;
    distr = distr_;
    que = que_;
    name = name_;
    m_num = num_;
}

static void backof_inc(std::string context, Ptr<const Packet> p)
{ all_backofs++; }

static void droped_inc(std::string context, Ptr<const Packet> p)
{ pack_droped_all++; }

int main(int argc, char **argv)
{
    LogComponentEnable("App", LOG_LEVEL_INFO);
    NodeContainer nodes;
    nodes.Create(CSMA_NUM);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(CH_DELAY)));
    csma.SetQueue("ns3::DropTailQueue");

    NetDeviceContainer devices = csma.Install(nodes);

    std::vector<Ptr<Queue<Packet>>> queues;
    for(uint32_t i = 0; i < CSMA_NUM; i++) {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(0.0000000001));
        Ptr<DropTailQueue<Packet>> que = CreateObject<DropTailQueue<Packet>>();
        que->SetMaxSize(QueueSize("50p"));
        que->TraceConnect("Drop", "Host " + std::to_string(i) + ": ", MakeCallback(&droped_inc));
        queues.push_back(que);
        devices.Get(i)->SetAttribute("TxQueue", PointerValue(que));
        devices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("12.21.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sink_port = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(CSMA_NUM - 1), sink_port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                       InetSocketAddress(Ipv4Address::GetAny(), sink_port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(CSMA_NUM - 1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(LIV_TIME));


    for(uint32_t i = 0; i < CSMA_NUM - 1; i++) {
        Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
        Ptr<App> app = CreateObject<App>();
        app->Init(ns3UdpSocket,
                  sinkAddress,
                  PCK_SIZE,
                  new generator(i),
                  new distribution(EXPONENT),
                  queues[i],
                  "Host " + std::to_string(i),
                  i);
        nodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(LIV_TIME));
        devices.Get(i)->TraceConnect("MacTxBackoff",
                                      "Host " + std::to_string(i) + ": ",
                                      MakeCallback(&backof_inc));
    }

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fifth.tr"));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    double aver_queue = 0;
    for(int t = 0; t < CSMA_NUM - 1; t++)
        aver_queue += double(queue_sum[t]) / double(pack_send[t]);
    double aver_backoff = (double) all_backofs / (pack_sent_all - pack_droped_all);

    std::cout << "Packets Sended:  " << pack_sent_all << std::endl
              << "Packets Droped:  " << pack_droped_all << std::endl
              << "Averege Backoff: " << aver_backoff << std::endl
              << "Averege Queue:   " << aver_queue / double(CSMA_NUM - 1) << std::endl
              << "Max Queue:       " << max_queue << std::endl;
    return 0;
}