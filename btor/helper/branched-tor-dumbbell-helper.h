
#ifndef BRANCHED_TORDUMBELLHELPER_H
#define BRANCHED_TORDUMBELLHELPER_H

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/global-route-manager.h"
#include "../model/pseudo-socket.h"
#include "../model/tor-base.h"


using namespace std;
namespace ns3 {

class CircuitDescriptor
  {
public:
    CircuitDescriptor ()
    {
    }

    CircuitDescriptor (int id, vector<string> nodeName, Ptr<PseudoSocket> clientSocket, string ctype)
    {
      this->id = id;
 
      for(size_t i=0;i<nodeName.size();i++){
      	  this->path.push_back(nodeName[i]);
      }
      this->m_clientSocket = clientSocket;
      this->circuit_type=ctype;
    }

    vector<string> getPath(){
    	return path;
    }

    string getCtype(){
    	return circuit_type;
    }

    int id;
    vector<string> path;
    Ptr<PseudoSocket> m_clientSocket;
    string circuit_type; 
  };

class RelaySupport
{
	  RelaySupport()
	  {
	  }

	  RelaySupport(string name, string continent,string bandwidth)
	  {
		  this->name=name;
		  this->continent=continent;
		  this->bandwidth=bandwidth;
	  }

	  string continent;
	  string name;
	  string bandwidth;

};

class BranchedTorDumbbellHelper
{
public:
   BranchedTorDumbbellHelper ();
  ~BranchedTorDumbbellHelper ();

  void ParseFile (string);

  void SetCircBranchLen(uint32_t len); 
  void SetCircChainLen(uint32_t len);  
  void SetNumCircBranch(uint32_t num);  
  void SetNumBranchWithChain(uint32_t num);  

  void SetMyIDApp (void); 

  void AddCircuit (int,string,vector<string>,vector<string>,vector<string>);

  void SetRelayAttribute (string, string, const AttributeValue &value);
  void SetStartTimeStream (Ptr<RandomVariableStream>);
  void SetRtt (Time);
  void DisableProxies (bool);
  void EnablePcap (bool);
  void EnableNscStack (bool,string = "cubic");
  void SetTorAppType (string);
  void BuildTopology ();

  ApplicationContainer GetTorAppsContainer ();

  void AssignIPAddress(void); 

  Ptr<Node> GetNode (string,uint32_t);
  Ptr<Node> GetNode (string);
  Ipv4Address GetIp (string,uint32_t);
  Ipv4Address GetIp (string);

  Ptr<Node> GetTorNode (string);
  Ptr<TorBaseApp> GetTorApp (string);

  vector<int> m_id_branch_circ; 
  map<int,vector<int>> m_list_circ_id_chain; 

  map<uint16_t,uint16_t> m_map_dest; 

  uint16_t m_chain_dest;
  uint16_t m_hidden_chain_dest;

private:

  class RelayDescriptor
  {
public:
    RelayDescriptor ()
    {
    }

    RelayDescriptor (string name, string continent, int spokeId, Ptr<TorBaseApp> app)
    {
      this->spokeId = spokeId;
      tapp = app;
      tapp->SetNodeName (name);
      this->continent = continent;
    }

    Ptr<TorBaseApp> tapp;
    string continent;
    int spokeId;
  };


  Ptr<TorBaseApp> CreateTorApp ();
  void AddRelay (string,string);
  void InstallCircuits ();
  Ptr<TorBaseApp> InstallTorApp (string);

  Ptr<PointToPointChannel> GetP2pChannel (RelayDescriptor);
  string GetContinent (string);

  map<int,CircuitDescriptor> m_circuits;
  map<string, RelayDescriptor> m_relays;

  int m_nSpokes;
  bool m_disableProxies;
  bool m_enablePcap;
  std::string m_nscTcpCong;
  std::string m_torAppType;
  Ptr<RandomVariableStream> m_startTimeStream;
  Ptr<UniformRandomVariable> m_rng;

  NodeContainer nodes; 
  size_t n_nodes; 

  uint32_t circ_branch_len; 
  uint32_t circ_chain_len;  
  uint32_t circ_branch_num; 
  uint32_t branch_with_chain_num; 

  ApplicationContainer m_relayApps;
  InternetStackHelper m_stackHelper;

  PointToPointDumbbellHelper *m_dumbbellHelper;

  int m_nLeftLeaf;
  int m_nRightLeaf;

  Ipv4AddressHelper m_leftIp;
  Ipv4AddressHelper m_rightIp;
  Ipv4AddressHelper m_routerIp;

  PointToPointHelper m_p2pLeftHelper;
  PointToPointHelper m_p2pRightHelper;
  PointToPointHelper m_p2pRouterHelper;

  Ptr<EmpiricalRandomVariable> m_owdLeft;
  Ptr<EmpiricalRandomVariable> m_owdRight;
  Ptr<EmpiricalRandomVariable> m_owdRouter;
  uint32_t m_routerDelay;

  Ptr<EmpiricalRandomVariable> m_clientBwLeft;
  Ptr<EmpiricalRandomVariable> m_clientBwRight;

  ObjectFactory m_factory;

};


} //end namespace ns3


#endif
