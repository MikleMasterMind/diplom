// per-packet-load-balancer.cc
#include "per-packet-load-balancer.h"

#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PerPacketLoadBalancer");
NS_OBJECT_ENSURE_REGISTERED (PerPacketLoadBalancer);

TypeId
PerPacketLoadBalancer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PerPacketLoadBalancer")
    .SetParent<Ipv4StaticRouting> ()
    .SetGroupName ("Internet")
    .AddConstructor<PerPacketLoadBalancer> ()
    .AddAttribute ("RoundRobinIndex",
                   "Current index for round-robin balancing",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PerPacketLoadBalancer::m_currentInterfaceIndex),
                   MakeUintegerChecker<uint32_t> ());
  return tid;
}

PerPacketLoadBalancer::PerPacketLoadBalancer ()
  : m_currentInterfaceIndex (0)
{
  NS_LOG_FUNCTION (this);
}

PerPacketLoadBalancer::~PerPacketLoadBalancer ()
{
  NS_LOG_FUNCTION (this);
}

std::string
PerPacketLoadBalancer::GetProtocolName (void) const
{
  return "PerPacketLoadBalancer";
}

std::vector<uint32_t>
PerPacketLoadBalancer::GetRouteInterfacesTo (Ipv4Address dest) const
{
  std::vector<uint32_t> interfaces;

  for (uint32_t i = 0; i < GetNRoutes (); i++)
    {
      Ipv4RoutingTableEntry route = GetRoute (i);

      // host route exact match
      if (route.GetDest () == dest)
        {
          interfaces.push_back (route.GetInterface ());
        }
      // network route match
      else if (route.GetDest ().CombineMask (route.GetDestNetworkMask ()) ==
               dest.CombineMask (route.GetDestNetworkMask ()))
        {
          interfaces.push_back (route.GetInterface ());
        }
    }

  return interfaces;
}

Ipv4Address
PerPacketLoadBalancer::GetGatewayForInterface (uint32_t interface, Ipv4Address dest) const
{
  for (uint32_t i = 0; i < GetNRoutes (); i++)
    {
      Ipv4RoutingTableEntry route = GetRoute (i);

      bool match = (route.GetDest () == dest) ||
                   (route.GetDest ().CombineMask (route.GetDestNetworkMask ()) ==
                    dest.CombineMask (route.GetDestNetworkMask ()));

      if (match && route.GetInterface () == interface)
        {
          return route.GetGateway ();
        }
    }
  return Ipv4Address::GetZero ();
}

// =====================
// RouteOutput: for locally generated packets (ACK spraying)
// =====================
Ptr<Ipv4Route>
PerPacketLoadBalancer::RouteOutput (Ptr<Packet> p,
                                   const Ipv4Header& header,
                                   Ptr<NetDevice> oif,
                                   Socket::SocketErrno& sockerr)
{
  NS_LOG_FUNCTION (this << header.GetDestination ());

  Ptr<Ipv4> ipv4 = m_ipv4;
  if (!ipv4)
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      return nullptr;
    }

  // If user forced output interface, fallback to base behavior
  if (oif)
    {
      return Ipv4StaticRouting::RouteOutput (p, header, oif, sockerr);
    }

  Ipv4Address dest = header.GetDestination ();

  std::vector<uint32_t> ifs = GetRouteInterfacesTo (dest);

  if (ifs.size () <= 1)
    {
      return Ipv4StaticRouting::RouteOutput (p, header, oif, sockerr);
    }

  // Make index safe even if number of routes changes
  m_currentInterfaceIndex %= ifs.size ();
  uint32_t selectedIf = ifs[m_currentInterfaceIndex];
  m_currentInterfaceIndex = (m_currentInterfaceIndex + 1) % ifs.size ();

  Ptr<NetDevice> outDev = ipv4->GetNetDevice (selectedIf);
  if (!outDev)
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      return nullptr;
    }

  // Source address: take first address on selected interface
  if (ipv4->GetNAddresses (selectedIf) == 0)
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      return nullptr;
    }

  Ipv4InterfaceAddress ifAddr = ipv4->GetAddress (selectedIf, 0);
  Ipv4Address gw = GetGatewayForInterface (selectedIf, dest);

  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetOutputDevice (outDev);
  rt->SetSource (ifAddr.GetLocal ());
  rt->SetDestination (dest);
  rt->SetGateway (gw);

  sockerr = Socket::ERROR_NOTERROR;
  return rt;
}

// =====================
// RouteInput: for forwarded traffic (balancer router case)
// =====================
bool
PerPacketLoadBalancer::RouteInput (Ptr<const Packet> p,
                                  const Ipv4Header& header,
                                  Ptr<const NetDevice> idev,
                                  const UnicastForwardCallback& ucb,
                                  const MulticastForwardCallback& mcb,
                                  const LocalDeliverCallback& lcb,
                                  const ErrorCallback& ecb)
{
  NS_LOG_FUNCTION (this << header.GetSource () << header.GetDestination ());

  Ptr<Ipv4> ipv4 = m_ipv4;
  if (!ipv4)
    {
      return Ipv4StaticRouting::RouteInput (p, header, idev, ucb, mcb, lcb, ecb);
    }

  Ipv4Address dst = header.GetDestination ();

  // Local delivery check: if any interface has dst address
  for (uint32_t i = 0; i < ipv4->GetNInterfaces (); i++)
    {
      for (uint32_t j = 0; j < ipv4->GetNAddresses (i); j++)
        {
          if (ipv4->GetAddress (i, j).GetLocal () == dst)
            {
              lcb (p, header, header.GetProtocol ());
              return true;
            }
        }
    }

  // Broadcast/multicast -> base
  if (dst.IsBroadcast () || dst.IsMulticast ())
    {
      return Ipv4StaticRouting::RouteInput (p, header, idev, ucb, mcb, lcb, ecb);
    }

  // Balance if multiple routes exist
  std::vector<uint32_t> ifs = GetRouteInterfacesTo (dst);

  if (ifs.size () <= 1)
    {
      return Ipv4StaticRouting::RouteInput (p, header, idev, ucb, mcb, lcb, ecb);
    }

  m_currentInterfaceIndex %= ifs.size ();
  uint32_t selectedIf = ifs[m_currentInterfaceIndex];
  m_currentInterfaceIndex = (m_currentInterfaceIndex + 1) % ifs.size ();

  Ptr<NetDevice> outDev = ipv4->GetNetDevice (selectedIf);
  if (!outDev)
    {
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return false;
    }

  if (ipv4->GetNAddresses (selectedIf) == 0)
    {
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return false;
    }

  Ipv4InterfaceAddress ifAddr = ipv4->GetAddress (selectedIf, 0);
  Ipv4Address gw = GetGatewayForInterface (selectedIf, dst);

  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetOutputDevice (outDev);
  rt->SetSource (ifAddr.GetLocal ());
  rt->SetDestination (dst);
  rt->SetGateway (gw);

  ucb (rt, p, header);
  return true;
}

void
PerPacketLoadBalancer::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  Ipv4StaticRouting::PrintRoutingTable (stream, unit);
}

} // namespace ns3
