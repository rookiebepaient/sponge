#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    //这个方法是在 TCPConnection 或者一个路由器想要发送一个 IP数据包到下一跳时被调用。
    //此接口工作就是转换这个数据包变成以太网帧并最终发送它

    //如果目标以太网地址已经知道，立马发送; ARP表中有目标MAC地址
    if (ip_mac_map.count(next_hop_ip)) {
        EthernetFrame addr_known_ethfrm;
        addr_known_ethfrm.header().type = EthernetHeader::TYPE_IPv4;
        addr_known_ethfrm.header().src = _ethernet_address;
        addr_known_ethfrm.header().dst = ip_mac_map[next_hop_ip].ethernet_addr;
        addr_known_ethfrm.payload() = dgram.serialize();
        _frames_out.push(addr_known_ethfrm);
    } else {  //如果不知道目标以太网地址，则广播ARP请求来确认下一跳的以太网地址，将IP数据报入队这样在收到ARP回复后可以马上发送该数据报
        //如果没有正在等待回复的ARP请求
        if (!_waiting_arp_response_map.count(next_hop_ip)) {
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;

            EthernetFrame addr_unknown_ethfrm;
            addr_unknown_ethfrm.header().type = EthernetHeader::TYPE_ARP;
            addr_unknown_ethfrm.header().src = _ethernet_address;
            addr_unknown_ethfrm.header().dst = ETHERNET_BROADCAST;

            addr_unknown_ethfrm.payload() = BufferList(arp_request.serialize());

            _frames_out.push(addr_unknown_ethfrm);
            //记录发向这个ip的ttl，如果上一个向相同IP地址发送的ARP请求还未过去5秒。则不需要再发送，继续等待第一个ARP请求的回复到来
            _waiting_arp_response_map.try_emplace(next_hop_ip, _default_arp_response_ttl);
        }
        //将IP数据报加入到队列中，当收到ARP回复后得到目标MAC地址后，及时将IP数据报发送
        _waiting_arp_internet_dgram.push_back({next_hop_ip, dgram});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //这个方法在当一个以太网帧到达网络时被调用，忽略那些目的地址不是本网络接口的以太网帧或是ARP请求报文
    if (frame.header().dst != _ethernet_address and frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }
    //当接受的帧时IPv4时，将有效载荷解析为 IP数据报
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram no_err_internet_dgram;
        if (no_err_internet_dgram.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }
        //如果解析成功返回IP数据报
        return no_err_internet_dgram;
    }
    //当接受的帧是ARP请求报文时
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) {
            return {};
        }
        bool is_arp_request =
            arp_msg.opcode == ARPMessage::OPCODE_REQUEST and arp_msg.target_ip_address == _ip_address.ipv4_numeric();
        bool is_arp_reply =
            arp_msg.opcode == ARPMessage::OPCODE_REPLY and arp_msg.target_ethernet_address == _ethernet_address;
        //如果是一个ARP请求报文 从sender字段中获取映射信息，并返回一个 ARP reply
        if (is_arp_request) {
            //调用 arp_reply() 方法
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            arp_reply.target_ip_address = arp_msg.sender_ip_address;

            EthernetFrame eth_fram_reply;
            eth_fram_reply.header().type = EthernetHeader::TYPE_ARP;
            eth_fram_reply.header().src = _ethernet_address;
            eth_fram_reply.header().dst = arp_msg.sender_ethernet_address;

            eth_fram_reply.payload() = arp_reply.serialize();

            _frames_out.push(eth_fram_reply);
        }
        if (is_arp_reply or is_arp_request) {
            //使用emplace 的效率比 insert 高
            ip_mac_map.try_emplace(
                arp_msg.sender_ip_address,
                NetworkInterface::ARP_Entry{arp_msg.sender_ethernet_address, _default_arp_entry_ttl});

            for (auto iter = _waiting_arp_internet_dgram.begin(); iter != _waiting_arp_internet_dgram.end();) {
                if (iter->first == arp_msg.sender_ip_address) {
                    //为什么再次发送IP数据报，因为没有得到目标MAC地址，所以在得到了ARP回复后再次重新发送IP数据报
                    send_datagram(iter->second, Address::from_ipv4_numeric(iter->first));
                    iter = _waiting_arp_internet_dgram.erase(iter);
                } else {
                    ++iter;
                }
            }
            //将等待回复的ARP请求清空
            _waiting_arp_response_map.erase(arp_msg.sender_ip_address);
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    //将ARP缓存中过期的条目删除
    for (auto iter = ip_mac_map.begin(); iter != ip_mac_map.end();) {
        if (iter->second.ttl <= ms_since_last_tick) {
            iter = ip_mac_map.erase(iter);
        } else {
            iter->second.ttl -= ms_since_last_tick;
            ++iter;
        }
    }

    //将ARP等待队列中过期的条目删除，超过5s的重传
    for (auto iter = _waiting_arp_response_map.begin(); iter != _waiting_arp_response_map.end();) {
        //超时了还没有等到ARP的回复
        if (iter->second <= ms_since_last_tick) {
            //重新发送ARP请求
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = iter->first;

            EthernetFrame rep_eth_fram;
            rep_eth_fram.header().type = EthernetHeader::TYPE_ARP;
            rep_eth_fram.header().src = _ethernet_address;
            rep_eth_fram.header().dst = ETHERNET_BROADCAST;

            rep_eth_fram.payload() = arp_request.serialize();

            _frames_out.push(rep_eth_fram);

            iter->second = _default_arp_response_ttl;
        } else {
            iter->second -= ms_since_last_tick;
            ++iter;
        }
    }
}
