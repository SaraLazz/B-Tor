//#define NS_LOG_APPEND_CONTEXT clog << Simulator::Now ().GetSeconds () << " ";

#include "ns3/log.h"
#include "ns3/random-variable-stream.h"

#include "tor.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TorApp");
NS_OBJECT_ENSURE_REGISTERED (TorApp);

TypeId
TorApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TorApp")
    .SetParent<TorBaseApp> ()
    .AddConstructor<TorApp> ()
    .AddAttribute ("WindowStart", "End-to-end sliding window size (in cells).",
                   IntegerValue (1000),
                   MakeIntegerAccessor (&TorApp::m_windowStart),
                   MakeIntegerChecker<int> ())
    .AddAttribute ("WindowIncrement", "End-to-end sliding window increment (in cells).",
                   IntegerValue (100),
                   MakeIntegerAccessor (&TorApp::m_windowIncrement),
                   MakeIntegerChecker<int> ());
  return tid;
}

TorApp::TorApp (void)
{
  listen_socket = 0;
  m_scheduleReadHead = 0;
  m_scheduleWriteHead = 0;
  pck_read=0;
}

TorApp::~TorApp (void)
{
  NS_LOG_FUNCTION (this);
}

void
TorApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  listen_socket = 0;

  map<uint16_t,Ptr<Circuit> >::iterator i;
  for (i = circuits.begin (); i != circuits.end (); ++i)
    {
      i->second->DoDispose ();
    }
  circuits.clear ();
  baseCircuits.clear ();
  connections.clear ();
  Application::DoDispose ();
}

void
TorApp::SetPckRead (void)
{
	pck_read=pck_read+1;
}




void
TorApp::AddCircuit (int id, string ctype, Ipv4Address n_ip, int n_conntype, Ipv4Address p_ip, int p_conntype,
                    Ptr<PseudoSocket> clientSocket)
{

  TorBaseApp::AddCircuit (id,ctype, n_ip, n_conntype, p_ip, p_conntype);

  // ensure unique id
  NS_ASSERT (circuits[id] == 0);

  // allocate and init new circuit
  Ptr<Connection> p_conn = AddConnection (p_ip, p_conntype);
  Ptr<Connection> n_conn = AddConnection (n_ip, n_conntype);
  p_conn->SetSocket (clientSocket);

  Ptr<Circuit> circ = Create<Circuit> (id,ctype, n_conn, p_conn, m_windowStart, m_windowIncrement);

  // add to circuit list maintained by every connection
  AddActiveCircuit (p_conn, circ);
  AddActiveCircuit (n_conn, circ);

  // add to the global list of circuits
  circuits[id] = circ;
  baseCircuits[id] = circ;
}



Ptr<Connection>
TorApp::AddConnection (Ipv4Address ip, int conntype)
{
  // find existing or create new connection
  Ptr<Connection> conn;
  vector<Ptr<Connection> >::iterator it;
  for (it = connections.begin (); it != connections.end (); ++it)
    {
      if ((*it)->GetRemote () == ip)
        {
          conn = *it;
          break;
        }
    }

  if (!conn)
    {
      conn = Create<Connection> (this, ip, conntype);
      connections.push_back (conn);
    }

  return conn;
}

void
TorApp::AddActiveCircuit (Ptr<Connection> conn, Ptr<Circuit> circ)
{
  NS_ASSERT (conn);
  NS_ASSERT (circ);
  if (conn)
    {
      if (!conn->GetActiveCircuits ()) 
        {
          conn->SetActiveCircuits (circ);
          circ->SetNextCirc (conn, circ); 
        }
      else
        { 
          Ptr<Circuit> temp = conn->GetActiveCircuits ()->GetNextCirc (conn);
          circ->SetNextCirc (conn, temp); 
          conn->GetActiveCircuits ()->SetNextCirc (conn, circ); 
        }
    }
}


void
TorApp::StartApplication (void)
{

  TorBaseApp::StartApplication ();
  m_readbucket.SetRefilledCallback (MakeCallback (&TorApp::RefillReadCallback, this));
  m_writebucket.SetRefilledCallback (MakeCallback (&TorApp::RefillWriteCallback, this));

  // create listen socket
  if (!listen_socket)
    {
      listen_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
      listen_socket->Bind (m_local);
      listen_socket->Listen ();
    }

  listen_socket->SetAcceptCallback (MakeNullCallback<bool,Ptr<Socket>,const Address &> (),
                                    MakeCallback (&TorApp::HandleAccept,this));

  Ipv4Mask ipmask = Ipv4Mask ("255.0.0.0");

  // iterate over all neighboring connections
  vector<Ptr<Connection> >::iterator it;
  for ( it = connections.begin (); it != connections.end (); it++ )
    {
      Ptr<Connection> conn = *it;
      NS_ASSERT (conn);

      // if m_ip smaller then connect to remote node
      if (m_ip < conn->GetRemote () && conn->SpeaksCells ())
        {
          Ptr<Socket> socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
          socket->Bind ();
          socket->Connect (Address (InetSocketAddress (conn->GetRemote (), InetSocketAddress::ConvertFrom (m_local).GetPort ())));
          socket->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
          socket->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
          conn->SetSocket (socket);
        }

      if (ipmask.IsMatch (conn->GetRemote (), Ipv4Address ("127.0.0.1")) )
        {
          if (conn->GetType () == SERVEREDGE)
            {
              Ptr<Socket> socket = CreateObject<PseudoServerSocket> ();
              socket->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
              socket->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
              conn->SetSocket (socket);
            }

          if (conn->GetType () == FAKE_SERVEREDGE)
          {
        	  Ptr<Socket> socket = CreateObject<PseudoBulkSocket> ();
        	  socket->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
        	  socket->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
        	  conn->SetSocket (socket);
          }

          if (conn->GetType () == PROXYEDGE)
            {
              Ptr<Socket> socket = conn->GetSocket ();
              if (!socket)
                {
                  socket = CreateObject<PseudoClientSocket> ();
                }

              socket->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
              socket->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
              conn->SetSocket (socket);
            }

          if (conn->GetType () == FAKE_PROXYEDGE) 
          {
        	  Ptr<Socket> socket = conn->GetSocket ();
        	  if (!socket)
        	  {
        		  socket = CreateObject<PseudoBulkSocket> (); 
        	  }

        	  socket->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
        	  socket->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
        	  conn->SetSocket (socket);
          }

        }
    }

}

void
TorApp::StopApplication (void)
{
  // close listen socket
  if (listen_socket)
    {
      listen_socket->Close ();
      listen_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }

  // close all connections
  vector<Ptr<Connection> >::iterator it_conn;
  for ( it_conn = connections.begin (); it_conn != connections.end (); ++it_conn )
    {
      Ptr<Connection> conn = *it_conn;
      NS_ASSERT (conn);
      if (conn->GetSocket ())
        {
          conn->GetSocket ()->Close ();
          conn->GetSocket ()->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
          conn->GetSocket ()->SetDataSentCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t > ());
        }
    }
}

Ptr<Circuit>
TorApp::GetCircuit (uint16_t circid)
{
  return circuits[circid];
}


void
TorApp::ConnReadCallback (Ptr<Socket> socket)
{

  NS_ASSERT (socket);
  Ptr<Connection> conn = LookupConn (socket);
  NS_ASSERT (conn);

  if (conn->IsBlocked ())
    {
      NS_LOG_LOGIC ("Reading blocked, return");
      return;
    }

  uint32_t base = conn->SpeaksCells () ? CELL_NETWORK_SIZE : CELL_PAYLOAD_SIZE;
  uint32_t max_read = RoundRobin (base, m_readbucket.GetSize ());

  // find the minimum amount of data to read safely from the socket
  max_read = min (max_read, socket->GetRxAvailable ());
  NS_LOG_LOGIC ("Read " << max_read << "/" << socket->GetRxAvailable () << " bytes from " << conn->GetRemote ());


  if (m_readbucket.GetSize() <= 0 && m_scheduleReadHead == 0) {
    m_scheduleReadHead = conn;
  }

  if (max_read <= 0)
    {
      return;
    }

  if (!conn->SpeaksCells ())
    {
      max_read = min (conn->GetActiveCircuits ()->GetPackageWindow () * base,max_read);
    }

  vector<Ptr<Packet> > packet_list;
  uint32_t read_bytes = conn->Read (&packet_list, max_read);

  for (uint32_t i = 0; i < packet_list.size (); i++)
  {
	  if (conn->SpeaksCells ())
	  {

		  if(packet_list[i]->GetSize()!=512) NS_LOG_INFO("SIZE="<<(unsigned int) packet_list[i]->GetSize());

		  		 
		  		  CellHeader h_temp;
		  		  packet_list[i]->PeekHeader (h_temp);

		  		  
		  		  uint16_t id_circ_temp=h_temp.GetCircId();
		  		  string ctype=circuits[id_circ_temp]->m_ctype;
		  		

		  		  if(h_temp.GetType()==REQUEST || h_temp.GetCmd()==RELAY_SENDME){
		  			  ReceiveRelayCell (conn, packet_list[i]);

		  			  if(ctype=="BRANCH"){
		  				  for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					  if(it->second->m_ctype=="CHAIN"){
		  						  uint16_t id_circ_temp_bridge=it->first;
		  						  BridgeRelayCell(conn,packet_list[i]->Copy(),id_circ_temp_bridge);
		  					  }
		  					  if(it->second->m_ctype=="HIDDEN_BRANCH"){
		  						  uint16_t id_circ_temp_bridge=it->first;
		  						  BridgeRelayCell(conn,packet_list[i]->Copy(),id_circ_temp_bridge);
		  					  }
		  				  }
		  				  
		  			  }
		  			  if(ctype=="CHAIN"){
		  				 for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					  if(it->second->m_ctype=="HIDDEN_CHAIN"){
		  						  uint16_t id_circ_temp_bridge=it->first;
		  						  BridgeRelayCell(conn,packet_list[i]->Copy(),id_circ_temp_bridge);
		  						  break;
		  					  }
		  				  }
		  			  }
		  		  }

		  		  if(h_temp.GetType()==RESPONSE){
		  			SetPckRead();

		  			  if(ctype=="BRANCH")
		  			  {
		  				  bool chain_found=false;
		  				  for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					  if(it->second->m_ctype=="CHAIN"){
		  						chain_found=true;
		  						  if(chain_dest!=it->first){ 
		  							  ReceiveRelayCell (conn, packet_list[i]); 	
		  						  }
		  						  break;
		  					  }
		  				  }
		  				if(!chain_found){ 
		  					ReceiveRelayCell (conn, packet_list[i]); 
		  				}
		  			  }

		  			  if(ctype=="CHAIN")
		  			  {
		  				  for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					  if(it->second->m_ctype=="BRANCH"){
		  						if(chain_dest==id_circ_temp){ 
		  							uint16_t id_circ_temp_bridge=it->first;
		  							BridgeRelayCell(conn,packet_list[i]->Copy(),id_circ_temp_bridge);
		  						}
		  						  break;
		  					  }
		  					  if(it->second->m_ctype=="HIDDEN_CHAIN"){
		  						 if(hidden_chain_dest!=it->first){ 
		  							ReceiveRelayCell (conn, packet_list[i]);
		  						 }
		  						  break;
		  					  }
		  					  //COMMENT THIS FOR B-TOR //////////////////////////
		  					  /*if(it->second->m_ctype=="CHAIN"){ //ONLY FOR L-TOR
		  						 if(hidden_chain_dest!=it->first){ 
		  							ReceiveRelayCell (conn, packet_list[i]);
		  						 }
		  						  break;
		  					  }*/
		  					  /////////////////////////////////////////////////////		  					  
		  				  }
		  			  }

		  			  if(ctype=="HIDDEN_BRANCH")
		  			  {
		  				  bool branch_found=false;
		  				  for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					  if(it->second->m_ctype=="BRANCH"){
		  						  branch_found=true;
		  						  break;
		  					  }
		  				  }
		  				  if(!branch_found){
		  					ReceiveRelayCell (conn, packet_list[i]);
		  				  }
		  				  
		  			  }

		  			  if(ctype=="HIDDEN_CHAIN")
		  			  {
		  				bool chain_found=false;
		  				uint16_t id_circ_temp_bridge=0;
		  				for (std::map<uint16_t,Ptr<Circuit>>::iterator it=circuits.begin(); it!=circuits.end(); ++it){
		  					if(it->second->m_ctype=="CHAIN"){
		  						
		  						chain_found=true;
		  						id_circ_temp_bridge=it->first;
		  						break;
		  					}
		  				}
		  				if(!chain_found){
		  					ReceiveRelayCell (conn, packet_list[i]);
		  				}
		  				else{ 
		  					if(hidden_chain_dest==id_circ_temp){ 
		  						BridgeRelayCell(conn,packet_list[i]->Copy(),id_circ_temp_bridge);
		  					}
		  				}
		  			  }


		  			  }
		  		  }
		  		  

	  else
	  {
		  SetPckRead();
		  PackageRelayCell (conn, packet_list[i]);
	  }
  }


  if (read_bytes > 0)
    {
      // decrement buckets
      GlobalBucketsDecrement (read_bytes, 0);

      // try to read more
      if (socket->GetRxAvailable () > 0)
        {
          // add some virtual processing time before reading more
          Time delay = Time::FromInteger (read_bytes * 2, Time::NS);
          conn->ScheduleRead (delay);
        }
    }
}


void
TorApp::BridgeRelayCell (Ptr<Connection> conn, Ptr<Packet> cell,uint16_t id_circ_bridge)
{
	CellHeader h;
	cell->RemoveHeader(h);
	h.SetCircId(id_circ_bridge);
	cell->AddHeader(h);

	Ptr<Circuit> circ=circuits[id_circ_bridge];
	CellDirection direction;

	if(h.GetType()==REQUEST || h.GetCmd()==RELAY_SENDME){
		direction=OUTBOUND;
	}
	if(h.GetType()==RESPONSE){
		direction=INBOUND;
	}


	Ptr<Connection> conn_bridge=circ->GetConnection(direction);
	NS_ASSERT(conn_bridge);
	NS_ASSERT(conn_bridge!=conn);
	AppendCellToCircuitQueue (circ, cell, direction);
}


void
TorApp::PackageRelayCell (Ptr<Connection> conn, Ptr<Packet> cell)
{
  NS_ASSERT (conn);
  NS_ASSERT (cell);
  Ptr<Circuit> circ = conn->GetActiveCircuits ();
  NS_ASSERT (circ);

  PackageRelayCellImpl (circ->GetId (), cell, conn->GetType());
  NS_ASSERT(cell->GetSize()==512);

  CellDirection direction = circ->GetOppositeDirection (conn);
  AppendCellToCircuitQueue (circ, cell, direction);
  if (circ->GetPackageWindow () <= 0)
    {
      NS_LOG_LOGIC ("[Circuit " << circ->GetId () << "] Package window empty. Block reading from " << conn->GetRemote ());
      conn->SetBlocked (true);
    }
}

void
TorApp::PackageRelayCellImpl (uint16_t circ_id, Ptr<Packet> cell,uint8_t conntype)
{
  NS_ASSERT (cell);
  CellHeader h;
  h.SetCircId (circ_id);
  h.SetCmd (RELAY_DATA);
  h.SetLength (cell->GetSize ());

  if(conntype==PROXYEDGE){
	  h.SetType(REQUEST);
  }
  if(conntype==SERVEREDGE){ 
	  Ptr<Circuit> circ = circuits[circ_id];
	  h.SetType(RESPONSE);
  }

  cell->AddHeader (h);
  NS_ASSERT(cell->GetSize()==512);
}


void
TorApp::ReceiveRelayCell (Ptr<Connection> conn, Ptr<Packet> cell)
{
  NS_ASSERT (conn);
  NS_ASSERT (cell);
  Ptr<Circuit> circ = LookupCircuitFromCell (cell);
  NS_ASSERT (circ);

  // find target connection for relaying
  CellDirection direction = circ->GetOppositeDirection (conn);
  Ptr<Connection> target_conn = circ->GetConnection (direction);

  if(target_conn->GetType()==PROXYEDGE){
	  CellHeader h;
	  cell->PeekHeader (h);
  }

  NS_ASSERT (target_conn);

  AppendCellToCircuitQueue (circ, cell, direction);
}


Ptr<Circuit>
TorApp::LookupCircuitFromCell (Ptr<Packet> cell)
{
  NS_ASSERT (cell);
  CellHeader h;
  cell->PeekHeader (h);
  return circuits[h.GetCircId ()];
}



void
TorApp::AppendCellToCircuitQueue (Ptr<Circuit> circ, Ptr<Packet> cell, CellDirection direction)
{
  NS_ASSERT (circ);
  NS_ASSERT (cell);
  queue<Ptr<Packet> > *queue = circ->GetQueue (direction);
  Ptr<Connection> conn = circ->GetConnection (direction);
  NS_ASSERT (queue);
  NS_ASSERT (conn);

  circ->PushCell (cell, direction);

  NS_LOG_LOGIC ("[Circuit " << circ->GetId () << "] Appended cell. Queue holds " << queue->size () << " cells.");
  conn->ScheduleWrite ();
}



void
TorApp::ConnWriteCallback (Ptr<Socket> socket, uint32_t tx)
{

  NS_ASSERT (socket);
  Ptr<Connection> conn = LookupConn (socket);
  NS_ASSERT (conn);

  uint32_t newtx = socket->GetTxAvailable ();

  int written_bytes = 0;
  uint32_t base = conn->SpeaksCells () ? CELL_NETWORK_SIZE : CELL_PAYLOAD_SIZE;
  uint32_t max_write = RoundRobin (base, m_writebucket.GetSize ());
  max_write = max_write > newtx ? newtx : max_write;

  NS_LOG_LOGIC ("Write max " << max_write << " bytes to " << conn->GetRemote ());

  if (m_writebucket.GetSize() <= 0 && m_scheduleWriteHead == 0) {
    m_scheduleWriteHead = conn;
  }

  if (max_write <= 0)
    {
      return;
    }

  written_bytes = conn->Write (max_write);
  NS_LOG_LOGIC (written_bytes << " bytes written to " << conn->GetRemote ());

  if (written_bytes > 0)
    {
      GlobalBucketsDecrement (0, written_bytes);

      /* try flushing more */
      conn->ScheduleWrite ();
    }
}



void
TorApp::HandleAccept (Ptr<Socket> s, const Address& from)
{
  Ptr<Connection> conn;
  Ipv4Address ip = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
  vector<Ptr<Connection> >::iterator it;
  for (it = connections.begin (); it != connections.end (); ++it)
    {
      if ((*it)->GetRemote () == ip && !(*it)->GetSocket () )
        {
          conn = *it;
          break;
        }
    }

  NS_ASSERT (conn);
  conn->SetSocket (s);

  s->SetRecvCallback (MakeCallback (&TorApp::ConnReadCallback, this));
  s->SetDataSentCallback (MakeCallback (&TorApp::ConnWriteCallback, this));
  conn->ScheduleWrite();
  conn->ScheduleRead();
}



Ptr<Connection>
TorApp::LookupConn (Ptr<Socket> socket)
{
  vector<Ptr<Connection> >::iterator it;
  for ( it = connections.begin (); it != connections.end (); it++ )
    {
      NS_ASSERT (*it);
      if ((*it)->GetSocket () == socket)
        {
          return (*it);
        }
    }
  return NULL;
}



void
TorApp::RefillReadCallback (int64_t prev_read_bucket)
{
  NS_LOG_LOGIC ("read bucket was " << prev_read_bucket << ". Now " << m_readbucket.GetSize ());
  if (prev_read_bucket <= 0 && m_readbucket.GetSize () > 0)
    {
      vector<Ptr<Connection> >::iterator it;
      vector<Ptr<Connection> >::iterator headit;

      headit = connections.begin();
      if (m_scheduleReadHead != 0) {
        headit = find(connections.begin(),connections.end(),m_scheduleReadHead);
        m_scheduleReadHead = 0;
      }

      it = headit;
      do {
        Ptr<Connection> conn = *it;
        NS_ASSERT(conn);
        conn->ScheduleRead (Time ("10ns"));
        if (++it == connections.end ()) {
          it = connections.begin ();
        }
      } while (it != headit);
    }
}


void
TorApp::RefillWriteCallback (int64_t prev_write_bucket)
{
  NS_LOG_LOGIC ("write bucket was " << prev_write_bucket << ". Now " << m_writebucket.GetSize ());

  if (prev_write_bucket <= 0 && m_writebucket.GetSize () > 0)
    {
      vector<Ptr<Connection> >::iterator it;
      vector<Ptr<Connection> >::iterator headit;

      headit = connections.begin();
      if (m_scheduleWriteHead != 0) {
        headit = find(connections.begin(),connections.end(),m_scheduleWriteHead);
        m_scheduleWriteHead = 0;
      }

      it = headit;
      do {
        Ptr<Connection> conn = *it;
        NS_ASSERT(conn);
        conn->ScheduleWrite ();
        if (++it == connections.end ()) {
          it = connections.begin ();
        }
      } while (it != headit);
    }
}


/** We just read num_read and wrote num_written bytes
 * onto conn. Decrement buckets appropriately. */
void
TorApp::GlobalBucketsDecrement (uint32_t num_read, uint32_t num_written)
{
  m_readbucket.Decrement (num_read);
  m_writebucket.Decrement (num_written);
}



/** Helper function to decide how many bytes out of global_bucket
 * we're willing to use for this transaction. Yes, this is how Tor
 * implements it; no kidding. */
uint32_t
TorApp::RoundRobin (int base, int64_t global_bucket)
{
  uint32_t num_bytes_high = 32 * base;
  uint32_t num_bytes_low = 4 * base;
  int64_t at_most = global_bucket / 8;
  at_most -= (at_most % base);

  if (at_most > num_bytes_high)
    {
      at_most = num_bytes_high;
    }
  else if (at_most < num_bytes_low)
    {
      at_most = num_bytes_low;
    }

  if (at_most > global_bucket)
    {
      at_most = global_bucket;
    }

  if (at_most < 0)
    {
      return 0;
    }
  return at_most;
}



Circuit::Circuit (uint16_t circ_id, string ctype, Ptr<Connection> n_conn, Ptr<Connection> p_conn,
                  int windowStart, int windowIncrement) : BaseCircuit (circ_id)
{
  this->p_cellQ = new queue<Ptr<Packet> >;
  this->n_cellQ = new queue<Ptr<Packet> >;

  m_windowStart = windowStart;
  m_windowIncrement = windowIncrement;
  this->deliver_window = m_windowStart;
  this->package_window = m_windowStart;

  this->p_conn = p_conn;
  this->n_conn = n_conn;

  this->next_active_on_n_conn = 0;
  this->next_active_on_p_conn = 0;

  m_ctype=ctype; 
}


Circuit::~Circuit ()
{
  NS_LOG_FUNCTION (this);
  delete this->p_cellQ;
  delete this->n_cellQ;
}

void
Circuit::DoDispose ()
{
  this->next_active_on_p_conn = 0;
  this->next_active_on_n_conn = 0;
  this->p_conn->SetActiveCircuits (0);
  this->n_conn->SetActiveCircuits (0);
}


Ptr<Packet>
Circuit::PopQueue (queue<Ptr<Packet> > *queue)
{
  if (queue->size () > 0)
    {
      Ptr<Packet> cell = queue->front ();
      queue->pop ();

      return cell;
    }
  return 0;
}


Ptr<Packet>
Circuit::PopCell (CellDirection direction)
{
  Ptr<Packet> cell;
  if (direction == OUTBOUND)
    {
      cell = this->PopQueue (this->n_cellQ);
    }
  else
    {
      cell = this->PopQueue (this->p_cellQ);
    }

  if (cell)
    {
      if (!IsSendme (cell))
        {
          IncrementStats (direction, 0, CELL_PAYLOAD_SIZE); 
        }

      /* handle sending sendme cells here (instead of in PushCell) because
       * otherwise short circuits could have more than a window-ful of cells
       * in-flight. Regular circuits will not be affected by this. */
      Ptr<Connection> conn = GetConnection (direction);
      if (!conn->SpeaksCells ())
        {
          deliver_window--;
          if (deliver_window <= m_windowStart - m_windowIncrement)
            {
              IncDeliverWindow ();
              NS_LOG_LOGIC ("[Circuit " << GetId () << "] Send SENDME cell ");
              Ptr<Packet> sendme_cell = CreateSendme ();
              GetQueue (BaseCircuit::GetOppositeDirection (direction))->push (sendme_cell);
              GetOppositeConnection (direction)->ScheduleWrite ();
            }
        }
    }

  return cell;
}


void
Circuit::PushCell (Ptr<Packet> cell, CellDirection direction)
{
  if (cell)
    {
      Ptr<Connection> conn = GetConnection (direction);
      Ptr<Connection> opp_conn = GetOppositeConnection (direction);

      if (!opp_conn->SpeaksCells ())
        {
          // new packaged cell
          package_window--;

          if (package_window <= 0)
            {
              //block connection
              opp_conn->SetBlocked (true);
            }
        }

      if (!conn->SpeaksCells ())
        {
          // delivery
          if (IsSendme (cell))
            {
              // update package window
              IncPackageWindow ();
              NS_LOG_LOGIC ("[Circuit " << GetId () << "] Received SENDME cell. Package window now " << package_window);
              if (conn->IsBlocked ())
                {
                  conn->SetBlocked (false);
                  conn->ScheduleRead ();
                }

              // no stats and no cell push on sendme cells
              return;
            }

          CellHeader h;
          cell->RemoveHeader (h);
        }

      IncrementStats (direction, CELL_PAYLOAD_SIZE, 0);
      GetQueue (direction)->push (cell);
    }
}





Ptr<Connection>
Circuit::GetConnection (CellDirection direction)
{
  if (direction == OUTBOUND)
    {
      return this->n_conn;
    }
  else
    {
      return this->p_conn;
    }
}

Ptr<Connection>
Circuit::GetOppositeConnection (CellDirection direction)
{
  if (direction == OUTBOUND)
    {
      return this->p_conn;
    }
  else
    {
      return this->n_conn;
    }
}

Ptr<Connection>
Circuit::GetOppositeConnection (Ptr<Connection> conn)
{
  if (this->n_conn == conn)
    {
      return this->p_conn;
    }
  else if (this->p_conn == conn)
    {
      return this->n_conn;
    }
  else
    {
      return 0;
    }
}



CellDirection
Circuit::GetDirection (Ptr<Connection> conn)
{
  if (this->n_conn == conn)
    {
      return OUTBOUND;
    }
  else
    {
      return INBOUND;
    }
}

CellDirection
Circuit::GetOppositeDirection (Ptr<Connection> conn)
{
  if (this->n_conn == conn)
    {
      return INBOUND;
    }
  else
    {
      return OUTBOUND;
    }
}

Ptr<Circuit>
Circuit::GetNextCirc (Ptr<Connection> conn)
{
  NS_ASSERT (this->n_conn);
  if (this->n_conn == conn)
    {
      return next_active_on_n_conn;
    }
  else
    {
      return next_active_on_p_conn;
    }
}


void
Circuit::SetNextCirc (Ptr<Connection> conn, Ptr<Circuit> circ)
{
  if (this->n_conn == conn)
    {
      next_active_on_n_conn = circ;
    }
  else
    {
      next_active_on_p_conn = circ;
    }
}


bool
Circuit::IsSendme (Ptr<Packet> cell)
{
  if (!cell)
    {
      return false;
    }
  CellHeader h;
  cell->PeekHeader (h);
  if (h.GetCmd () == RELAY_SENDME)
    {
      return true;
    }
  return false;
}

Ptr<Packet>
Circuit::CreateSendme ()
{
  CellHeader h;
  h.SetCircId (GetId ());
  h.SetType (RELAY);
  h.SetStreamId (42);
  h.SetCmd (RELAY_SENDME);
  h.SetLength (0);
  Ptr<Packet> cell = Create<Packet> (CELL_PAYLOAD_SIZE);
  cell->AddHeader (h);

  return cell;
}


queue<Ptr<Packet> >*
Circuit::GetQueue (CellDirection direction)
{
  if (direction == OUTBOUND)
    {
      return this->n_cellQ;
    }
  else
    {
      return this->p_cellQ;
    }
}


uint32_t
Circuit::GetQueueSize (CellDirection direction)
{
  if (direction == OUTBOUND)
    {
      return this->n_cellQ->size ();
    }
  else
    {
      return this->p_cellQ->size ();
    }
}

uint32_t
Circuit::GetPackageWindow ()
{
  return package_window;
}

void
Circuit::IncPackageWindow ()
{
  package_window += m_windowIncrement;
  if (package_window > m_windowStart)
    {
      package_window = m_windowStart;
    }
}

uint32_t
Circuit::GetDeliverWindow ()
{
  return deliver_window;
}

void
Circuit::IncDeliverWindow ()
{
  deliver_window += m_windowIncrement;
  if (deliver_window > m_windowStart)
    {
      deliver_window = m_windowStart;
    }
}

uint32_t
Circuit::SendCell (CellDirection direction)
{
  queue<Ptr<Packet> >* cellQ = GetQueue (direction);
  if (cellQ->size () <= 0)
    {
      return 0;
    }

  Ptr<Connection> conn = GetConnection (direction);
  if (conn->IsBlocked () || conn->GetSocket ()->GetTxAvailable () < CELL_NETWORK_SIZE)
    {
      return 0;
    }

  Ptr<Packet> cell = PopCell (direction);
  return conn->GetSocket ()->Send (cell);
}



Connection::Connection (TorApp* torapp, Ipv4Address ip, int conntype)
{
  this->torapp = torapp;
  this->remote = ip;
  this->inbuf.size = 0;
  this->outbuf.size = 0;
  this->reading_blocked = 0;
  this->active_circuits = 0;

  m_socket = 0;
  m_conntype = conntype;
}


Connection::~Connection ()
{
  NS_LOG_FUNCTION (this);
}

Ptr<Circuit>
Connection::GetActiveCircuits ()
{
  return active_circuits;
}

void
Connection::SetActiveCircuits (Ptr<Circuit> circ)
{
  active_circuits = circ;
}


uint8_t
Connection::GetType ()
{
  return m_conntype;
}

bool
Connection::SpeaksCells ()
{
  return m_conntype == RELAYEDGE;
}

bool
Connection::IsBlocked ()
{
  return reading_blocked;
}

void
Connection::SetBlocked (bool b)
{
  reading_blocked = b;
}


Ptr<Socket>
Connection::GetSocket ()
{
  return m_socket;
}

void
Connection::SetSocket (Ptr<Socket> socket)
{
  m_socket = socket;
}

Ipv4Address
Connection::GetRemote ()
{
  return remote;
}


uint32_t
Connection::Read (vector<Ptr<Packet> >* packet_list, uint32_t max_read)
{
  if (reading_blocked)
    {
      return 0;
    }

  uint8_t raw_data[max_read + this->inbuf.size];
  memcpy (raw_data, this->inbuf.data, this->inbuf.size);
  int read_bytes = m_socket->Recv (&raw_data[this->inbuf.size], max_read, 0);

  uint32_t base = SpeaksCells () ? CELL_NETWORK_SIZE : CELL_PAYLOAD_SIZE;
  uint32_t datasize = read_bytes + inbuf.size;
  uint32_t leftover = datasize % base;
  int num_packages = datasize / base;

  // slice data into packets
  Ptr<Packet> cell;
  for (int i = 0; i < num_packages; i++)
    {
      cell = Create<Packet> (&raw_data[i * base], base);
      packet_list->push_back (cell);
    }

  //safe leftover
  memcpy (inbuf.data, &raw_data[datasize - leftover], leftover);
  inbuf.size = leftover;

  return read_bytes;
}


uint32_t
Connection::Write (uint32_t max_write)
{
  uint32_t base = SpeaksCells () ? CELL_NETWORK_SIZE : CELL_PAYLOAD_SIZE;
  uint8_t raw_data[outbuf.size + (max_write / base + 1) * base];
  memcpy (raw_data, outbuf.data, outbuf.size);
  uint32_t datasize = outbuf.size;
  int written_bytes = 0;

  // fill raw_data
  bool flushed_some = false;
  Ptr<Circuit> start_circ = GetActiveCircuits ();
  NS_ASSERT (start_circ);
  Ptr<Circuit> circ;
  Ptr<Packet> cell = Ptr<Packet> (NULL);
  CellDirection direction;

  while (datasize < max_write)
    {
      circ = GetActiveCircuits ();
      NS_ASSERT (circ);

      direction = circ->GetDirection (this);
      cell = circ->PopCell (direction);

      if (cell)
        {
          datasize += cell->CopyData (&raw_data[datasize], cell->GetSize ());
          flushed_some = true;
        }

      SetActiveCircuits (circ->GetNextCirc (this));

      if (GetActiveCircuits () == start_circ)
        {
          if (!flushed_some)
            {
              break;
            }
          flushed_some = false;
        }
    }

  // send data
  max_write = min (max_write, datasize);
  if (max_write > 0)
    {
      written_bytes = m_socket->Send (raw_data, max_write, 0);
    }

  /* save leftover for next time */
  written_bytes = max (written_bytes,0);
  uint32_t leftover = datasize - written_bytes;
  memcpy (outbuf.data, &raw_data[datasize - leftover], leftover);
  outbuf.size = leftover;

  return written_bytes;
}


void
Connection::ScheduleWrite (Time delay)
{
  if (m_socket && write_event.IsExpired ())
    {
      write_event = Simulator::Schedule (delay, &TorApp::ConnWriteCallback, torapp, m_socket, m_socket->GetTxAvailable ());
    }
}

void
Connection::ScheduleRead (Time delay)
{
  if (m_socket && read_event.IsExpired ())
    {
      read_event = Simulator::Schedule (delay, &TorApp::ConnReadCallback, torapp, m_socket);
    }
}


uint32_t
Connection::GetOutbufSize ()
{
  return outbuf.size;
}

uint32_t
Connection::GetInbufSize ()
{
  return inbuf.size;
}

} //namespace ns3
