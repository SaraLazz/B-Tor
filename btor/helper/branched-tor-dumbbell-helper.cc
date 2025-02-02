#include <iostream>
#include <fstream>
#include <sstream>

#include <string>
#include <unistd.h>

#include "branched-tor-dumbbell-helper.h"

using namespace std;

namespace ns3 {

BranchedTorDumbbellHelper::BranchedTorDumbbellHelper ()
{

	  m_owdLeft = CreateObject<EmpiricalRandomVariable> ();
	  m_owdLeft->CDF ( 11 / 2.0,0.00);
	  m_owdLeft->CDF ( 29 / 2.0,0.25);
	  m_owdLeft->CDF ( 45 / 2.0,0.50);
	  m_owdLeft->CDF ( 73 / 2.0,0.75);
	  m_owdLeft->CDF (148 / 2.0,1.00);
	  uint32_t leftDelay = m_owdLeft->GetInteger ();

	  m_owdRight = CreateObject<EmpiricalRandomVariable> ();
	  m_owdRight->CDF ( 14 / 2.0,0.00);
	  m_owdRight->CDF ( 27 / 2.0,0.25);
	  m_owdRight->CDF ( 37 / 2.0,0.50);
	  m_owdRight->CDF ( 48 / 2.0,0.75);
	  m_owdRight->CDF ( 65 / 2.0,1.00);
	  uint32_t rightDelay = m_owdRight->GetInteger ();

	  m_owdRouter = CreateObject<EmpiricalRandomVariable> ();
	  m_owdRouter->CDF ( 78 / 2.0,0.00);
	  m_owdRouter->CDF (117 / 2.0,0.25);
	  m_owdRouter->CDF (132 / 2.0,0.50);
	  m_owdRouter->CDF (161 / 2.0,0.75);
	  m_owdRouter->CDF (252 / 2.0,1.00);
	  m_routerDelay = m_owdRouter->GetInteger ();
	  m_routerDelay = max ((int)m_routerDelay - (int)(rightDelay + leftDelay), 1);

	  m_p2pLeftHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (leftDelay)));

	  m_p2pRightHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (rightDelay)));

	  m_p2pRouterHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (m_routerDelay)));

	  m_clientBwRight = CreateObject<EmpiricalRandomVariable> ();
	  m_clientBwRight->CDF ( 1,0.00);
	  m_clientBwRight->CDF ( 4,0.14);
	  m_clientBwRight->CDF (10,0.69);
	  m_clientBwRight->CDF (15,0.86);
	  m_clientBwRight->CDF (47,1.00);

	  m_clientBwLeft = CreateObject<EmpiricalRandomVariable> ();
	  m_clientBwLeft->CDF ( 1,0.00);
	  m_clientBwLeft->CDF ( 4,0.24);
	  m_clientBwLeft->CDF (10,0.56);
	  m_clientBwLeft->CDF (15,0.78);
	  m_clientBwLeft->CDF (54,1.00);

	  m_p2pLeftHelper.SetDeviceAttribute ("DataRate", StringValue ("10Gb/s"));
	  m_p2pRightHelper.SetDeviceAttribute ("DataRate", StringValue ("10Gb/s"));
	  m_p2pRouterHelper.SetDeviceAttribute ("DataRate", StringValue ("10Gb/s"));

	  m_dumbbellHelper = 0;
	  m_nLeftLeaf = 0;
	  m_nRightLeaf = 0;

	  m_startTimeStream = 0;
  m_rng = CreateObject<UniformRandomVariable> ();
  m_disableProxies = false; 
  m_enablePcap = false;
  m_factory.SetTypeId ("ns3::TorApp");

}

BranchedTorDumbbellHelper::~BranchedTorDumbbellHelper ()
{
	if (m_dumbbellHelper)
	    {
	      delete m_dumbbellHelper;
	    }
}

void
BranchedTorDumbbellHelper::SetCircBranchLen(uint32_t len){ 
	circ_branch_len=len;
}

void
BranchedTorDumbbellHelper::SetCircChainLen(uint32_t len){ 
	circ_chain_len=len;
}

void
BranchedTorDumbbellHelper::SetNumCircBranch(uint32_t num){ 
	circ_branch_num=num;
}

void
BranchedTorDumbbellHelper::SetNumBranchWithChain(uint32_t num){ 
	branch_with_chain_num=num;
}


void
BranchedTorDumbbellHelper::AddCircuit (int id,string circuit_type, vector<string> nodeName,
		vector<string> nodeContinent, vector<string> nodeBandwidth)
{

  NS_ASSERT (m_circuits.find (id) == m_circuits.end()); 

  Ptr<PseudoSocket> clientSocket;

  if(circuit_type=="BRANCH") {
	  Ptr<PseudoClientSocket> clientSocket_temp = CreateObject<PseudoClientSocket> ();
	  if (m_startTimeStream)
	  {
		  clientSocket_temp->Start (Seconds (1)); 
	  }
	  clientSocket=clientSocket_temp;
  }
  else{
	  Ptr<PseudoBulkSocket>  clientSocket_temp = CreateObject<PseudoBulkSocket>();
	  clientSocket=clientSocket_temp;
  }
  
  CircuitDescriptor desc (id, nodeName, clientSocket, circuit_type);
  m_circuits[id] = desc;

  for(size_t i=0;i<nodeName.size();i++){
	  AddRelay (nodeName[i],nodeContinent[i]);
	  SetRelayAttribute (nodeName[i], "BandwidthRate", DataRateValue (DataRate (nodeBandwidth[i]+ "B/s")));
	  SetRelayAttribute (nodeName[i], "BandwidthBurst", DataRateValue (DataRate (nodeBandwidth[i] + "B/s")));
  }

}


void
BranchedTorDumbbellHelper::AddRelay (string name,string continent)
{
  if (m_relays.find (name) == m_relays.end ()) 
    {

      if (continent.size () == 0)
        {
          continent = m_rng->GetValue () < 0.5 ? "NA" : "EU";
        }

      RelayDescriptor desc;
            if (continent == "NA")
              {
                desc = RelayDescriptor (name, continent, m_nLeftLeaf++, CreateTorApp ());
              }
            else
              {
                desc = RelayDescriptor (name, continent, m_nRightLeaf++, CreateTorApp ());
              }

      m_relays[name] = desc;
      m_relayApps.Add (desc.tapp); 
    }
}


void
BranchedTorDumbbellHelper::SetRelayAttribute (string relayName, string attrName, const AttributeValue &value)
{
  NS_ASSERT (m_relays.find (relayName) != m_relays.end ());
  GetTorApp (relayName)->SetAttribute (attrName, value);
}

void
BranchedTorDumbbellHelper::SetStartTimeStream (Ptr<RandomVariableStream> startTimeStream)
{
  m_startTimeStream = startTimeStream;
}

void
BranchedTorDumbbellHelper::EnableNscStack (bool enableNscStack, string nscTcpCong)
{
  if (enableNscStack)
    {
      m_nscTcpCong = nscTcpCong;
    }
  else
    {
      m_nscTcpCong = "";
    }
}

void
BranchedTorDumbbellHelper::EnablePcap (bool enablePcap)
{
  m_enablePcap = enablePcap;
}

void
BranchedTorDumbbellHelper::SetTorAppType (string type)
{
  m_factory.SetTypeId (type);
}

void
BranchedTorDumbbellHelper::DisableProxies (bool disableProxies)
{
  m_disableProxies = disableProxies;
}

void
BranchedTorDumbbellHelper::ParseFile (string filename)
{
	ifstream f;
	f.open (filename.c_str (), ios::in);
	NS_ASSERT (f.is_open ());

	while (!f.eof ())
	{
		string line;
		getline (f, line);
		if (line == "")
		{
			break;
		}

		istringstream iss (line);
		string element;
		vector<string> row;

		while (iss >> element)
		{
			row.push_back (element);
			//j++;
		}

		uint32_t circ_len=0;

		int id=stoi(row[0]);
		string type=row[1];

		if(row[1]=="BRANCH"){
			circ_len=circ_branch_len;
		}
		if(row[1]=="CHAIN"){
			circ_len=circ_chain_len;
			m_chain_dest=id;
		}
		if(row[1]=="HIDDEN_BRANCH"){
			circ_len=4;
		}
		if(row[1]=="HIDDEN_CHAIN"){
			circ_len=3;
			m_hidden_chain_dest=id;
		}

		NS_ASSERT(circ_len>0);

		string name;
		string continent;
		string bandwidth;

		vector<string> nodeName;
		vector<string> nodeContinent;
		vector<string> nodeBandwidth;

		for(uint32_t j=2;j<row.size();j=j+3)
		{
			NS_ASSERT(j+1<row.size());
			NS_ASSERT(j+2<row.size());
			name=row[j];
			continent=row[j+1];
			bandwidth=row[j+2];

			///////////////////////////
			int band_temp=stoi(bandwidth);
			band_temp=404690;
			//band_temp=4046;
			string band_str=to_string(band_temp);
			///////////////////////////////////////////

			nodeName.push_back(name);
			nodeContinent.push_back(continent);
			nodeBandwidth.push_back(band_str);
		}

		if(type=="BRANCH"){

			for(int ind=0;ind<3;ind++){ 

			name="UNIQUE_PROXY_"+to_string(ind);
			continent=m_rng->GetValue () < 0.5 ? "NA" : "EU";

			continent="EU";

			Ptr<EmpiricalRandomVariable> euBW = CreateObject<EmpiricalRandomVariable> ();
			euBW->CDF ( 1,0.00);
			euBW->CDF ( 4,0.14);
			euBW->CDF (10,0.69);
			euBW->CDF (15,0.86);
			euBW->CDF (47,1.00);

			Ptr<EmpiricalRandomVariable> naBW = CreateObject<EmpiricalRandomVariable> ();
			naBW->CDF ( 1,0.00);
			naBW->CDF ( 4,0.24);
			naBW->CDF (10,0.56);
			naBW->CDF (15,0.78);
			naBW->CDF (54,1.00);

			if(continent=="NA"){
				bandwidth=to_string((naBW->GetInteger()*1000000));
			}
			else{
				bandwidth=to_string((euBW->GetInteger()*1000000));
			}

			int band_temp=404690;
			//int band_temp=4046;
			string bandwidth=to_string(band_temp);
			nodeName.insert(nodeName.begin(),name);
			nodeContinent.insert(nodeContinent.begin(),continent);
			nodeBandwidth.insert(nodeBandwidth.begin(),bandwidth);
		}
		}

		AddCircuit(id,type,nodeName,nodeContinent,nodeBandwidth); 
		nodeName.clear();
		nodeContinent.clear();
		nodeBandwidth.clear();
	}
}



void
BranchedTorDumbbellHelper::BuildTopology ()
{
  m_dumbbellHelper = new PointToPointDumbbellHelper (m_nLeftLeaf, m_p2pLeftHelper, m_nRightLeaf, m_p2pRightHelper, m_p2pRouterHelper);

  m_stackHelper.Install (m_dumbbellHelper->GetLeft ());
  m_stackHelper.Install (m_dumbbellHelper->GetRight ());

  for (int i = 0; i < m_nLeftLeaf; ++i)
    {
	  
	  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
	  urv->SetAttribute("Min", DoubleValue(0.0));
	  urv->SetAttribute("Max", DoubleValue(1.0));
	  
	  Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel> (
	      "RanVar", PointerValue(urv),
	      "ErrorRate", DoubleValue (0.00001));
	  GetNode ("NA",i)->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

      Ptr<PointToPointChannel> ch = GetNode ("NA",i)->GetDevice (0)->GetObject<PointToPointNetDevice> ()->GetChannel ()->GetObject<PointToPointChannel> ();
      ch->SetAttribute ("Delay", TimeValue (MilliSeconds (41)));
      m_stackHelper.Install (m_dumbbellHelper->GetLeft (i));
    }

  for (int i = 0; i < m_nRightLeaf; ++i)
    {

	  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
	  urv->SetAttribute("Min", DoubleValue(0.0));
	  urv->SetAttribute("Max", DoubleValue(1.0));
	  
	  Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel> (
	      "RanVar", PointerValue(urv),
	      "ErrorRate", DoubleValue (0.00001));
	  GetNode ("EU",i)->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));


      Ptr<PointToPointChannel> ch = GetNode ("EU",i)->GetDevice (0)->GetObject<PointToPointNetDevice> ()->GetChannel ()->GetObject<PointToPointChannel> ();
      ch->SetAttribute ("Delay", TimeValue (MilliSeconds (41)));
      m_stackHelper.Install (m_dumbbellHelper->GetRight (i));
    }

  //assign ipv4
  m_routerIp.SetBase ("10.1.0.0", "255.255.255.253");
  m_leftIp.SetBase ("10.2.0.0", "255.255.255.0");
  m_rightIp.SetBase ("10.128.0.0", "255.255.255.0");
  m_dumbbellHelper->AssignIpv4Addresses (m_leftIp, m_rightIp, m_routerIp);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  InstallCircuits ();
}



void
BranchedTorDumbbellHelper::InstallCircuits ()
{

  Ipv4AddressHelper ipHelper = Ipv4AddressHelper ("127.0.0.0", "255.0.0.0");

  map<int,CircuitDescriptor>::iterator i;
  for (i = m_circuits.begin (); i != m_circuits.end (); ++i)
    {
	  CircuitDescriptor e = i->second;

	  vector<string> path=e.getPath();

	  vector<Ptr<TorBaseApp>> appList;

	      for(size_t j=0;j<path.size();j++){
	    	  Ptr<TorBaseApp> app_temp=InstallTorApp(path[j]);
	    	  appList.push_back(app_temp);
	      }

	      for(size_t j=0;j<path.size();j++){

	    	  Ipv4Address nodeAddress_prec;
	    	  if(j==0){
	    		  Ipv4Address clientAddress = ipHelper.NewAddress ();
	    		  nodeAddress_prec = clientAddress;
	    	  }
	    	  else{
	    		  nodeAddress_prec = GetIp(path[j-1]);
	    	  }

	    	  Ipv4Address nodeAddress_suc;
	    	  if(j==path.size()-1){
	    		  Ipv4Address serverAddress = ipHelper.NewAddress (); 
	    		  nodeAddress_suc = serverAddress;
	    	  }
	    	  else{
	    		  nodeAddress_suc = GetIp(path[j+1]);
	    	  }

	    	  if(j == path.size()-1){
	    		  if(e.getCtype()=="HIDDEN_BRANCH" || e.getCtype()=="HIDDEN_CHAIN") appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, SERVEREDGE, nodeAddress_prec, RELAYEDGE); //for b-tor
	    		  //if(e.getCtype()=="CHAIN") appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, SERVEREDGE, nodeAddress_prec, RELAYEDGE); //for l-tor
	    		  else appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, RELAYEDGE, nodeAddress_prec, RELAYEDGE);
	    	  }
	    	  else{
	    		  if(j==0){
	    			  if(e.getCtype()=="BRANCH") appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, RELAYEDGE, nodeAddress_prec, PROXYEDGE, e.m_clientSocket);
	    			  else appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, RELAYEDGE, nodeAddress_prec, RELAYEDGE, e.m_clientSocket);
	    		  }
	    		  else{
	    			  appList[j]->AddCircuit (e.id,e.getCtype(), nodeAddress_suc, RELAYEDGE, nodeAddress_prec, RELAYEDGE);
	    		  }
	    	  }
	      }

	  }
}


Ptr<PointToPointChannel>
BranchedTorDumbbellHelper::GetP2pChannel(RelayDescriptor desc)
{
  Ptr<Node> node = GetNode (desc.continent,desc.spokeId);
  return node->GetDevice (0)->GetObject<PointToPointNetDevice> ()->GetChannel ()->GetObject<PointToPointChannel> ();
}

Ptr<Node>
BranchedTorDumbbellHelper::GetNode (string continent, uint32_t id)
{
  if (continent == "NA")
    {
      return m_dumbbellHelper->GetLeft (id);
    }
  else
    {
      return m_dumbbellHelper->GetRight (id);
    }
}

Ptr<Node>
BranchedTorDumbbellHelper::GetNode (string name)
{
  RelayDescriptor desc = m_relays[name];
  return GetNode (desc.continent,desc.spokeId);
}


string
BranchedTorDumbbellHelper::GetContinent (string name)
{
  return m_relays[name].continent;
}

Ipv4Address
BranchedTorDumbbellHelper::GetIp (string continent, uint32_t id)
{
  if (continent == "NA")
    {
      return m_dumbbellHelper->GetLeftIpv4Address (id);
    }
  else
    {
      return m_dumbbellHelper->GetRightIpv4Address (id);
    }
}

Ipv4Address
BranchedTorDumbbellHelper::GetIp (string name)
{
  RelayDescriptor desc = m_relays[name];
  return GetIp (desc.continent,desc.spokeId);
}


Ptr<TorBaseApp>
BranchedTorDumbbellHelper::InstallTorApp (string name)
{
  NS_ASSERT (m_relays.find (name) != m_relays.end ());
  RelayDescriptor desc = m_relays[name];
  if (GetNode (name)->GetNApplications () == 0 )
    {
      GetNode (name)->AddApplication (desc.tapp);
    }
  return desc.tapp;
}

void
BranchedTorDumbbellHelper::SetMyIDApp(void)
{
	map<string, RelayDescriptor>::iterator it=m_relays.begin();
	for (; it!=m_relays.end(); ++it){
		it->second.tapp->chain_dest=m_chain_dest;
		it->second.tapp->hidden_chain_dest=m_hidden_chain_dest;
	}
}



ApplicationContainer
BranchedTorDumbbellHelper::GetTorAppsContainer ()
{
  NS_ASSERT(m_relayApps.GetN()>0);
  return m_relayApps;
}

Ptr<TorBaseApp>
BranchedTorDumbbellHelper::GetTorApp (string name)
{
  return m_relays[name].tapp;
}



Ptr<TorBaseApp>
BranchedTorDumbbellHelper::CreateTorApp ()
{
  Ptr<TorBaseApp> tapp = m_factory.Create<TorBaseApp> ();
  NS_ASSERT (tapp);
  return tapp;
}


}


