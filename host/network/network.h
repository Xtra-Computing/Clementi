//Create network stack in FPGA side.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

#include "../log/log.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_ip.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"


/////////////////// FpgaIP Definition ///////////////
class FpgaIP {
protected:
    xrt::ip ip;
    std::unordered_map<std::string, uint32_t> registers_map;
public:
    FpgaIP(const xrt::device &device, const xrt::uuid &uuid, std::string ip_name);
    int writeRegister(const std::string &reg, uint32_t value);
    int writeRegisterAddr(uint32_t reg_addr, uint32_t value);
    uint32_t readRegister(const std::string &reg);
    uint32_t readRegister_offset(const std::string &reg, int offset);
};

/////////////////// AlveoVnxCmac Definition ///////////////
class AlveoVnxCmac : public FpgaIP {
public:
    AlveoVnxCmac(const xrt::device &device, const xrt::uuid &uuid, uint32_t inst_id);
    int AlveoVnxCmacReset();
    uint32_t getCmacPackets();
};

/////////////////// AlveoVnxNetworkLayer Definition ///////////////
class AlveoVnxNetworkLayer : public FpgaIP {
public:
    AlveoVnxNetworkLayer(const xrt::device &device, const xrt::uuid& uuid, uint32_t inst_id);
    int setSocket(const std::string &remote_ip, uint32_t remote_udp, uint32_t local_udp, int socket_index);
    int runARPDiscovery();
    bool IsARPTableFound(const std::string &dest_ip);
    int getSocketTable();
    int invalidSocketTable();
    int setAddress(const std::string &ip_address, const std::string &mac_address);
};