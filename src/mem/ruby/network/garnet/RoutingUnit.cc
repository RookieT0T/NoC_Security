/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include <random>

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn,
                            bool is_infected,
                            float probability_misroute)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeInfected(route, inport, inport_dirn,
                                   is_infected, probability_misroute); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            //assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            //assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            //assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            //assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    // check that the outport dirn's connecting router is not disabled
    // if it is, then we must reroute closest to the final destination
    if (is_next_router_disabled(outport_dirn))
        outport_dirn = reroute_dirn(dest_id, false, inport_dirn, outport_dirn);

    return m_outports_dirn2idx[outport_dirn];
}

// The routing algorithm for the infected router.
// Purposefully misroutes packets
// away from their destination
int
RoutingUnit::outportComputeInfected(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn,
                                 bool is_infected,
                                 float probability_misroute)
{
    // Get the direction that we SHOULD take,
    // if we weren't infected. assuming XY DOR
    int xy_outport = lookupRoutingTable(route.vnet, route.net_dest);
                    //outportComputeXY(route, inport, inport_dirn);

    // If the destination is local, take that always
    if (m_outports_idx2dirn[xy_outport].compare("Local") == 0)
        return xy_outport;

    // If we are not actually infected, we
    // can return the correct direction
    if (!is_infected)
        return xy_outport;

    // If the probability to misroute is 0,
    // do not attempt a reroute, return now
    if (probability_misroute == 0.0f)
        return xy_outport;

    // Now choose a direction from the remaining ports
    // and go that way (misroute)
    // within some degree of probability.
    // We do not wish to misroute all the time,
    // otherwise it would be too
    // obvious that we are malicious.
    std::random_device rd; std::mt19937 gen(0);
    std::uniform_real_distribution<> dis_f(0, 1);

    // Roll against the probability that we misroute this packet or not
    float roll = dis_f(gen);
    if (roll > probability_misroute)
        return xy_outport;

    // Won the roll, now misroute
    int misroute_outport;
    do {
        PortDirection misroute_dirn = reroute_dirn(route.dest_router,
                                            true,
                                            inport_dirn,
                                            m_outports_idx2dirn[xy_outport]);
        misroute_outport = m_outports_dirn2idx[misroute_dirn];
    } while (!is_next_router_disabled(m_outports_idx2dirn[misroute_outport]));

    return misroute_outport;
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    panic("%s placeholder executed", __FUNCTION__);
}

// detect if the router we are about to move to is
// disabled by the LFSR or not
bool
RoutingUnit::is_next_router_disabled(PortDirection outport_dirn)
{
    // get current lfsr state as a bitstream
    GarnetNetwork* gn = m_router->get_net_ptr();
    uint16_t lfsr_state = gn->m_lfsr->generate16Bitstream(1);

    // get the router ID for the next router in this path
    int my_id = m_router->get_id();
    int next_id = -1;
    if (outport_dirn.compare("North") == 0)
        next_id = my_id - gn->getNumCols();
    else if (outport_dirn.compare("South") == 0)
        next_id = my_id + gn->getNumCols();
    else if (outport_dirn.compare("East") == 0)
        next_id = my_id + 1;
    else if (outport_dirn.compare("West") == 0)
        next_id = my_id - 1;
    else // destination is not another router
        return false;

    // convert the next router ID into a onehot vector
    uint16_t next_id_onehot = 0;
    if (next_id != 0)
        next_id_onehot = 1 << (next_id-1);

    // compare the lfsr state to the onehot vector
    // if they overlap, then the next router is disabled
    // otherwise we are free to take this routing path
    return (lfsr_state & next_id_onehot) != 0;
}

PortDirection
RoutingUnit::reroute_dirn(int dest_id,
                        bool do_misroute,
                        PortDirection inport_dirn,
                        PortDirection prev_outport_dirn)
{
    int num_cols = m_router->get_net_ptr()->getNumCols();
    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    // first rerouting decision, unaware of mesh corners/edges
    PortDirection new_dirn = "Unknown";
    if (prev_outport_dirn.compare("North") == 0 ||
        prev_outport_dirn.compare("South") == 0) {
        if (my_x < dest_x)
            new_dirn = "East";
        else
            new_dirn = "West";
    } else if (prev_outport_dirn.compare("East") == 0 ||
             prev_outport_dirn.compare("West") == 0) {
        if (my_y < dest_y)
            new_dirn = "South";
        else
            new_dirn = "North";
    }

    // attempt to misroute if we are infected
    if (do_misroute) {
        if (new_dirn.compare("North") == 0)
            new_dirn = "South";
        else if (new_dirn.compare("South") == 0)
            new_dirn = "North";
        if (new_dirn.compare("East") == 0)
            new_dirn = "West";
        else if (new_dirn.compare("West") == 0)
            new_dirn = "East";
    }

    // we must now check for edges and corners, and reroute accordingly
    // U-turns may be necessary (unfortunate, but I am unsure how to stall)
    if (my_x == num_cols-1 && new_dirn.compare("East") == 0)
        new_dirn = "West";
    else if (my_x == 0 && new_dirn.compare("West") == 0)
        new_dirn = "East";
    if (my_y == num_cols-1 && new_dirn.compare("North") == 0)
        new_dirn = "South";
    else if (my_y == 0 && new_dirn.compare("South") == 0)
        new_dirn = "North";

    return new_dirn;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
