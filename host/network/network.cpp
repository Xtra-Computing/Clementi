//Create network stack in FPGA side.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include "network.h"

// XRT includes
#include "experimental/xrt_bo.h"
#include "experimental/xrt_ip.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"


/////////////////// FpgaIP Definition ///////////////
FpgaIP::FpgaIP(const xrt::device &device, const xrt::uuid &uuid, std::string ip_name) {
    this->ip = xrt::ip(device, uuid, ip_name);
}

int FpgaIP::writeRegister(const std::string &reg, uint32_t value) {
    if (this->registers_map.find(reg) == this->registers_map.end()) {
        std::cerr << "ERR: FpgaIP: register " << reg << " not found in the registers map" << std::endl;
        return -1;
    } else {
        this->ip.write_register(this->registers_map[reg], value);
        return 0;
    }
}

int FpgaIP::writeRegisterAddr(uint32_t reg_addr, uint32_t value) {
    this->ip.write_register(reg_addr, value);
    return 0;
}

uint32_t FpgaIP::readRegister(const std::string &reg) {
    if (this->registers_map.find(reg) == this->registers_map.end()) {
        std::cerr << "ERR: FpgaIP: register " << reg << " not found in the registers map" << std::endl;
        return -1;
    } else {
        return this->ip.read_register(this->registers_map[reg]);
    }
}

uint32_t FpgaIP::readRegister_offset(const std::string &reg, int offset) {
    if (this->registers_map.find(reg) == this->registers_map.end()) {
        std::cerr << "ERR: FpgaIP: register " << reg << " not found in the registers map" << std::endl;
        return -1;
    } else {
        return this->ip.read_register(this->registers_map[reg] + offset);
    }
}

/////////////////// AlveoVnxCmac Definition ///////////////
AlveoVnxCmac::AlveoVnxCmac(const xrt::device &device, const xrt::uuid &uuid, uint32_t inst_id) :
        FpgaIP::FpgaIP(device, uuid, "cmac_" + std::to_string(inst_id)) {
    this->registers_map["gt_reset_reg"] = 0x0000;
    this->registers_map["reset_reg"] = 0x0004;
    this->registers_map["stat_rx_status"] = 0x0204;
    this->registers_map["stat_rx_total_packets"] = 0x0608;
}

int AlveoVnxCmac::AlveoVnxCmacReset() { // temporally unused
    this->writeRegister("gt_reset_reg", 1);
    this->writeRegister("reset_reg", 1);
    usleep(1000);
    this->writeRegister("gt_reset_reg", 0);
    this->writeRegister("reset_reg", 0);
    return 0;
}

uint32_t AlveoVnxCmac::getCmacPackets() {
    uint32_t number_packets = this->readRegister("stat_rx_total_packets");
    return number_packets;
}

/////////////////// AlveoVnxNetworkLayer Definition ///////////////
AlveoVnxNetworkLayer::AlveoVnxNetworkLayer(const xrt::device &device, const xrt::uuid& uuid, uint32_t inst_id) :
        FpgaIP::FpgaIP(device, uuid, "networklayer:{networklayer_" + std::to_string(inst_id) + "}") {
    // Attention : need to keep consistent with address in kernel.xml.
    this->registers_map["my_mac_lsb"] = 0x0010;
    this->registers_map["my_mac_msb"] = 0x0014;
    this->registers_map["my_ip"] = 0x0018;

    this->registers_map["arp_discovery"] = 0x1010;
    this->registers_map["arp_valid_offset"] = 0x1100;
    this->registers_map["arp_ip_addr_offset"] = 0x1400;
    this->registers_map["arp_mac_addr_offset_lsb"] = 0x1800;
    this->registers_map["arp_mac_addr_offset_msb"] = 0x1804;

    this->registers_map["eth_in_packets"] = 0x0410;
    this->registers_map["eth_out_packets"] = 0x04B8;
    this->registers_map["pkth_in_packets"] = 0x0428;

    this->registers_map["udp_theirIP_offset"] = 0x0810;
    this->registers_map["udp_theirPort_offset"] = 0x0890;
    this->registers_map["udp_myPort_offset"] = 0x0910;
    this->registers_map["udp_valid_offset"] = 0x0990;
    this->registers_map["udp_number_offset"] = 0x0A10;
}

int AlveoVnxNetworkLayer::setSocket(const std::string &remote_ip, uint32_t remote_udp, uint32_t local_udp, int socket_index) {
    // conert IPv4 address string into 32b hex
    uint32_t a, b, c, d;
    char dot;
    std::stringstream ss(remote_ip);
    ss >> a >> dot >> b >> dot >> c >> dot >> d;
    uint32_t ip_hex = (a << 24) | (b << 16) | (c << 8) | d;

    // store the socket addresses in socket region with concecutive index
    this->writeRegisterAddr(this->registers_map["udp_theirIP_offset"] + socket_index * 8, ip_hex);
    this->writeRegisterAddr(this->registers_map["udp_theirPort_offset"] + socket_index * 8, remote_udp);
    this->writeRegisterAddr(this->registers_map["udp_myPort_offset"] + socket_index * 8, local_udp);
    this->writeRegisterAddr(this->registers_map["udp_valid_offset"] + socket_index * 8, 1);

    return 0;
}

int AlveoVnxNetworkLayer::runARPDiscovery() {
    this->writeRegister("arp_discovery", 0);
    this->writeRegister("arp_discovery", 1);
    this->writeRegister("arp_discovery", 0);
    return 0;
}

bool AlveoVnxNetworkLayer::IsARPTableFound(const std::string &dest_ip) {
    uint32_t valid_entry = 0, valid = 0;
    uint64_t mac_lsb, mac_msb, arp_ip;
    uint64_t mac_addr; // mac addr is used for debug

    uint32_t a, b, c, d;
    char dot;
    std::stringstream ss(dest_ip);
    ss >> a >> dot >> b >> dot >> c >> dot >> d;
    uint32_t dest_ip_dec = (a << 24) | (b << 16) | (c << 8) | d;

    for (int i = 0; i < 256; i++) { // max 256, only return valid arp table.
        if (i % 4 == 0)
            valid_entry = this->readRegister_offset("arp_valid_offset", (i / 4) * 4);

        valid = (valid_entry >> ((i % 4) * 8)) & 0x1;
        if (valid == 1) {
            mac_lsb = this->readRegister_offset("arp_mac_addr_offset_lsb", (i * 2 * 4));
            mac_msb = this->readRegister_offset("arp_mac_addr_offset_msb", ((i * 2 + 1) * 4));
            arp_ip  = this->readRegister_offset("arp_ip_addr_offset", (i * 4));
            mac_addr =  (mac_msb << 32) + mac_lsb;

            uint32_t ip_addr[4];
            ip_addr[0] = (arp_ip & 0xff000000) >> 24;
            ip_addr[1] = (arp_ip &   0xff0000) >> 16;
            ip_addr[2] = (arp_ip &     0xff00) >> 8;
            ip_addr[3] =  arp_ip &       0xff;

            log_debug(" ARP ip_addr %d.%d.%d.%d", ip_addr[3], ip_addr[2], ip_addr[1], ip_addr[0]);
            uint32_t temp = ip_addr[3] * 256 * 256 *256 + ip_addr[2] * 256 * 256 + ip_addr[1] * 256 + ip_addr[0];
            // some problems in MAC address
            log_debug(" mac_addr %x ... %x mac address %x", mac_msb, mac_lsb, mac_addr);
            if (temp == dest_ip_dec) {
                log_debug("arp find dest ip %s", dest_ip.c_str());
                return true;
            }
        }
    }
    return false;
}

int AlveoVnxNetworkLayer::getSocketTable() {
    // need to get it iteratively
    uint32_t socket_num = this->readRegister("udp_number_offset");
    uint32_t udp_their_ip, udp_their_port, udp_our_port, valid;

    for (uint32_t i = 0; i < socket_num; i++) {
        udp_their_ip    =  this->readRegister_offset("udp_theirIP_offset"   , i * 8);
        udp_their_port  =  this->readRegister_offset("udp_theirPort_offset" , i * 8);
        udp_our_port    =  this->readRegister_offset("udp_myPort_offset"    , i * 8);
        valid           =  this->readRegister_offset("udp_valid_offset"     , i * 8);

        if (valid == 1) { // only print valid ip port and address
            uint32_t ip_addr[4];
            ip_addr[0] = (udp_their_ip & 0xff000000) >> 24;
            ip_addr[1] = (udp_their_ip &   0xff0000) >> 16;
            ip_addr[2] = (udp_their_ip &     0xff00) >> 8;
            ip_addr[3] =  udp_their_ip &       0xff;

            log_debug("Socket table[%d] Their ip_addr %d.%d.%d.%d Their port %d Our Port %d", \
                        i, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3], udp_their_port, udp_our_port);    
        }
    }
    return 0;
}

int AlveoVnxNetworkLayer::invalidSocketTable() {
    uint32_t socket_num = this->readRegister("udp_number_offset");
    uint32_t udp_their_ip, udp_their_port, udp_our_port, valid;

    for (uint32_t i = 0; i < socket_num; i++) {
        udp_their_ip    =  this->readRegister_offset("udp_theirIP_offset"   , i * 8);
        udp_their_port  =  this->readRegister_offset("udp_theirPort_offset" , i * 8);
        udp_our_port    =  this->readRegister_offset("udp_myPort_offset"    , i * 8);
        valid           =  this->readRegister_offset("udp_valid_offset"     , i * 8);

        if (valid == 1) { // only print valid ip port and address
            this->writeRegisterAddr(this->registers_map["udp_valid_offset"] + i * 8, 0);
            uint32_t ip_addr[4];
            ip_addr[0] = (udp_their_ip & 0xff000000) >> 24;
            ip_addr[1] = (udp_their_ip &   0xff0000) >> 16;
            ip_addr[2] = (udp_their_ip &     0xff00) >> 8;
            ip_addr[3] =  udp_their_ip &       0xff;

            log_debug("Invalid Socket table[%d] Their ip_addr %d.%d.%d.%d Their port %d Our port %d",\
                         i, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3], udp_their_port, udp_our_port);       
        }
    }
    return 0;
}

int AlveoVnxNetworkLayer::setAddress(const std::string &ip_address, const std::string &mac_address) {

    // conert IPv4 address string into 32b hex
    uint64_t a, b, c, d;
    char dot;
    std::stringstream ss_ip(ip_address);
    ss_ip >> a >> dot >> b >> dot >> c >> dot >> d;
    // std::cout <<' '<< a <<' '<< b <<' '<< c <<' '<< d  <<std::endl;
    uint32_t ip_hex = (a << 24) | (b << 16) | (c << 8) | d;

    this->writeRegister("my_ip", ip_hex);
    // uint32_t ip_add = this->nl->readRegister("my_ip");
    // std::cout << std::hex <<"ip_addr is "<< ip_add <<std::endl;

    // conert MAC address string into 48b hex
    std::stringstream ss_mac(mac_address);
    // std::cout<<"mac_adress "<< mac_address <<std::endl;
    std::string t = mac_address;
    std::string mac_temp = t.replace(2,1,"");
    mac_temp = t.replace(4,1,"");
    mac_temp = t.replace(6,1,"");
    mac_temp = t.replace(8,1,"");
    mac_temp = t.replace(10,1,"");
    // std::cout << "mac_temp" <<mac_temp<<std::endl; // 000a35029dc9;
    uint64_t hex_mac = 0;
    for (int i = 0; i < 12; i++) {
        char temp = mac_temp.at(i);
        if ((temp <= 'f') && (temp >= 'a')) hex_mac += ((temp - 'a' + 10UL) << (11 - i)*4);
        if ((temp <= 'F') && (temp >= 'A')) hex_mac += ((temp - 'A' + 10UL) << (11 - i)*4);
        if ((temp <= '9') && (temp >= '0')) hex_mac += ((temp - '0' +  0UL) << (11 - i)*4);
    }
    // ss_mac >> a >> dot >> b >> dot >> c >> dot >> d >> dot >> e >> dot >> f;
    // std::cout <<' '<< a <<' '<< b <<' '<< c <<' '<< d  <<' '<< e <<' '<< f <<' '<<std::endl;
    // uint64_t hex_mac = (a << 40) | (b << 32) | (c << 24) | (d << 16) | (e << 8) | f;

    // std::cout<<"mac addr is "<<this->mac<<std::endl;

    this->writeRegister("my_mac_msb", static_cast<uint32_t>((hex_mac >> 32)& 0x0000ffff)); //add a mask
    this->writeRegister("my_mac_lsb", static_cast<uint32_t>(hex_mac & 0xffffffff));
    // std::cout <<" my_msb is "<< static_cast<uint32_t>(hex_mac >> 32) << std::endl;
    // std::cout <<" my lsb is "<< static_cast<uint32_t>(hex_mac & 0xffffffff) <<std::endl;

    // uint32_t mac_msb = this->nl->readRegister("my_mac_msb");
    // uint32_t mac_lsb = this->nl->readRegister("my_mac_lsb");
    // std::cout << "mac_msb is "<< mac_msb << " mac_lsb is "<< mac_lsb <<std::endl;

    return 0;
}