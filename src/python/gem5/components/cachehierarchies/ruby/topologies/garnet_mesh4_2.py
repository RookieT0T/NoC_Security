from m5.objects import (
    GarnetExtLink,
    GarnetIntLink,
    GarnetNetwork,
    GarnetNetworkInterface,
    GarnetRouter,
)

class Ring(GarnetNetwork):
    def __init__(self, ruby_system):
        super().__init__()
        self.ruby_system = ruby_system

    def connectControllers(
        self, l1i_ctrls, l1d_ctrls, mem_ctrls
    ):
        linkLatency = 3     # customizable
        routerLatency = 1

        self.coreRouters = [GarnetRouter(router_id = i, latency = routerLatency) for i in range(len(l1i_ctrls))]
        # external links
        # connect the two L1 controllers to each core router
        self.l1i_ext_links = [
            GarnetExtLink(link_id = i, ext_node = controller_i, int_node = self.coreRouters[i], latency = linkLatency)
            for i, controller_i in enumerate(l1i_ctrls)
        ]
        self.l1d_ext_links = [
            GarnetExtLink(link_id = len(l1i_ctrls) + i, ext_node = controller_data, int_node = self.coreRouters[i], latency = linkLatency)
            for i, controller_data in enumerate(l1d_ctrls)
        ]

        # connect the memory controller
        self.mem_ext_links = [
            GarnetExtLink(link_id = len(l1i_ctrls) * 2 + i, ext_node = controller_mem, int_node = self.coreRouters[0], latency = linkLatency)
            for i, controller_mem in enumerate(mem_ctrls)
        ]

        # internal links: a 4-ary 2-cube mesh
        self.int_links = []
        newLinkID = len(l1i_ctrls) * 2 + len(l2_ctrls) + len(mem_ctrls)
        numRows = 4
        assert (len(l1i_ctrls) % numRows) != 0, "Special Handles required for indivisible core numbers"
        numCols = int(len(l1i_ctrls) / numRows)
       
        for row in range(numRows):
            for col in range(numCols):
                if(col + 1 < numCols):
                    # west to east
                    westOut = row * numCols + col
                    eastIn = row * numCols + col + 1
                    self.int_links.append(GarnetIntLink(link_id = newLinkID, 
                                                        src_node = self.l1_routers[westOut],
                                                        dst_node = self.l1_routers[eastIn],
                                                        src_outport = "West",
                                                        dst_inport = "East",
                                                        latency = linkLatency,
                                                        weight = 1))
                    newLinkID += 1
                
                    # east to west
                    eastOut = row * numCols + col + 1
                    westIn = row * numCols + col
                    self.int_links.append(GarnetIntLink(link_id = newLinkID, 
                                                        src_node = self.l1_routers[eastOut],
                                                        dst_node = self.l1_routers[westIn],
                                                        src_outport = "East",
                                                        dst_inport = "West",
                                                        latency = linkLatency,
                                                        weight = 1))
                    newLinkID += 1

    
        for col in range(numCols):
            for row in range(numRows):
                if(row + 1 < numRows):
                    # north to south
                    northOut = row * numCols + col
                    southIn = (row + 1) * numCols + col
                    self.int_links.append(GarnetIntLink(link_id = newLinkID, 
                                                        src_node = self.l1_routers[northOut],
                                                        dst_node = self.l1_routers[southIn],
                                                        src_outport = "North",
                                                        dst_inport = "South",
                                                        latency = linkLatency,
                                                        weight = 2))
                    newLinkID += 1

                    # south to north
                    southOut = (row + 1) * numCols + col
                    northIn = row * numCols + col
                    self.int_links.append(GarnetIntLink(link_id = newLinkID, 
                                                        src_node = self.l1_routers[southOut],
                                                        dst_node = self.l1_routers[northIn],
                                                        src_outport = "South",
                                                        dst_inport = "North",
                                                        latency = linkLatency,
                                                        weight = 2))
                    newLinkID += 1

        self.ext_links = (self.l1i_ext_links + self.l1d_ext_links + self.mem_ext_links)
        self.netifs = [GarnetNetworkInterface(id = i) for (i,n) in enumerate(self.ext_links)]
        self.routers = (self.coreRouters)