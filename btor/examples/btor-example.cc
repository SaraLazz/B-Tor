/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"

#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

#include <iostream>
#include <fstream>

#include "../helper/branched-tor-dumbbell-helper.h"
#include "ns3/btor-module.h"

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("BTorExample");


int main (int argc, char *argv[]) {

    LogComponentEnable ("PseudoSocket", LOG_LEVEL_INFO);
    LogComponentEnable ("TorApp", LOG_LEVEL_INFO);
    LogComponentEnable ("TorBaseApp", LOG_LEVEL_INFO);
    LogComponentEnable ("BTorExample", LOG_LEVEL_INFO);

    uint32_t run = 1;
    Time simTime = Time("1000s");
    uint32_t branch_len=3; //Choose the branch length
    uint32_t chain_len=3;  //Choose the chain length
    //uint32_t len=12;
    std::string prova="5"; 

    CommandLine cmd;
    cmd.AddValue("branch_len", "branch len", branch_len);
    cmd.AddValue("chain_len", "chain len", chain_len);
    cmd.AddValue("prova", "prova", prova);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed (12);
    SeedManager::SetRun (run);
    
    //Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpWestwood")); //TO TRY A DIFFERENT TCP CONFIGURATION
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

    Config::SetDefault ("ns3::TorApp::WindowStart", IntegerValue (1000));
    Config::SetDefault ("ns3::TorApp::WindowIncrement", IntegerValue (100));

    NS_LOG_INFO("setup topology");

    BranchedTorDumbbellHelper th;
    Ptr<UniformRandomVariable> m_startTime = CreateObject<UniformRandomVariable> ();
    m_startTime->SetAttribute ("Min", DoubleValue (0.1));
    m_startTime->SetAttribute ("Max", DoubleValue (30.0));
    th.SetStartTimeStream (m_startTime);

    th.SetCircBranchLen(branch_len);
    th.SetCircChainLen(chain_len); 
    th.SetNumCircBranch(1); 

    string folder_name=to_string(branch_len)+"X"+to_string(chain_len); //for btor
    //string folder_name="z-"+to_string(len);  //for l-tor
    string file_name="res_"+prova+".txt.txt";

    th.ParseFile ("src/btor/doc2/EXP/"+folder_name+"/"+file_name); // parse scenario from file 

    th.BuildTopology(); // finally build topology, setup relays and seed circuits

    th.SetMyIDApp();

    ApplicationContainer relays = th.GetTorAppsContainer();
    relays.Start (Seconds (0.1));
    relays.Stop (simTime);

    Simulator::Stop (simTime);

    NS_LOG_INFO("start simulation");
    Simulator::Run ();

    NS_LOG_INFO("stop simulation");
    Simulator::Destroy ();

    return 0;
}



