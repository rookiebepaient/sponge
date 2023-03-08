#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.
    _routing_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.
    //如果到达这个路由器的数据报已经是 ttl 为1，那么说明它不可以再转发出去了
    // if (dgram.header().ttl <= 1) {
    //     return;
    // }
    //路由器搜索路由表找到匹配IP数据报中目的地址的路由路线。
    //通过最长前缀来进行匹配目标地址的 最高位前缀长度 位
    const uint32_t dst_ip_addr = dgram.header().dst;
    auto max_matched_entry = _routing_table.end();
    for (auto iter = _routing_table.begin(); iter != _routing_table.end(); iter++) {
        //通过 CIDR 中前缀长度 prefix_length 求出 子网掩码 mask
        uint32_t mask = (iter->_prefix_length) == 0 ? 0 : (0xffffffff << (32 - iter->_prefix_length));
        //选中具有最大长度匹配的路由表项
        if (iter->_prefix_length == 0 or
            (dst_ip_addr & mask) ==
                iter->_route_prefix  //利用IP数据报的目标IP地址和掩码mask做按位与运算得出结果看是否与路由表中的网络地址route_prefix相同
            /*((iter->_route_prefix ^ dst_ip_addr) >> (32 - iter->_prefix_length) == 0)*/) {
            // cout << "路由表中的网络地址(十进制) : " << iter->_route_prefix << " , 点分十进制 : " <<
            // Address::from_ipv4_numeric(iter->_route_prefix).ip()
            // << " , 数据报中目标IP地址 : " << dst_ip_addr << " , 点分十进制 : " <<
            // Address::from_ipv4_numeric(dst_ip_addr).ip() << endl;
            if (max_matched_entry == _routing_table.end() or max_matched_entry->_prefix_length < iter->_prefix_length) {
                max_matched_entry = iter;
            }
        }
    }

    if (max_matched_entry != _routing_table.end() and dgram.header().ttl-- > 1) {
        const auto next_hop = max_matched_entry->_next_hop;
        if (next_hop.has_value()) {
            interface(max_matched_entry->_interface_num).send_datagram(dgram, next_hop.value());
        } else {
            interface(max_matched_entry->_interface_num).send_datagram(dgram, Address::from_ipv4_numeric(dst_ip_addr));
        }
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
