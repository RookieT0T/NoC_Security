from m5.objects.ClockedObject import ClockedObject
from m5.params import *


class LFSR64(ClockedObject):
    type = "LFSR64"
    cxx_header = "mem/ruby/network/garnet/LFSR64.hh"
    cxx_class = "gem5::ruby::garnet::LFSR64"

    seed = Param.Int(1, "Seed for the LFSR")
    latency = Param.Cycles(10, "Cycles between shifting LFSR")
