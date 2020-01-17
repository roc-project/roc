/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <CppUTest/TestHarness.h>

#include "roc_address/parse_socket_addr.h"

namespace roc {
namespace address {

TEST_GROUP(socket_addr_parse) {};

TEST(socket_addr_parse, miface_ipv4) {
    SocketAddr addr;

    CHECK(addr.set_host_port(IPv4, "225.1.2.3", 123));
    CHECK(addr.has_host_port());
    CHECK(addr.multicast());

    CHECK(parse_socket_addr_miface("0.0.0.0", addr));
    CHECK(addr.has_miface());

    char miface[64];
    CHECK(addr.get_miface(miface, sizeof(miface)));
    STRCMP_EQUAL("0.0.0.0", miface);
}

TEST(socket_addr_parse, miface_ipv6) {
    SocketAddr addr;

    CHECK(addr.set_host_port(IPv6, "ffaa::", 123));
    CHECK(addr.has_host_port());
    CHECK(addr.multicast());

    CHECK(parse_socket_addr_miface("[2001:db8::1]", addr));
    CHECK(addr.has_miface());

    char miface[64];
    CHECK(addr.get_miface(miface, sizeof(miface)));
    STRCMP_EQUAL("2001:db8::1", miface);
}

TEST(socket_addr_parse, bad_miface) {
    { // invalid address
        SocketAddr addr;
        CHECK(!parse_socket_addr_miface("0.0.0.0", addr));
    }
    { // non-multicast address
        SocketAddr addr;

        CHECK(addr.set_host_port(IPv6, "2001:db8::1", 123));
        CHECK(addr.has_host_port());
        CHECK(!addr.multicast());

        CHECK(!parse_socket_addr_miface("[::]", addr));
    }
    { // empty miface
        SocketAddr addr;

        CHECK(addr.set_host_port(IPv4, "225.1.2.3", 123));
        CHECK(addr.has_host_port());

        CHECK(!parse_socket_addr_miface("", addr));
        CHECK(!parse_socket_addr_miface(NULL, addr));
    }
    { // ipv6 miface for ipv4 addr
        SocketAddr addr;

        CHECK(addr.set_host_port(IPv4, "225.1.2.3", 123));
        CHECK(addr.has_host_port());
        CHECK(addr.multicast());

        CHECK(!parse_socket_addr_miface("[::]", addr));
    }
    { // ipv4 miface for ipv6 addr
        SocketAddr addr;

        CHECK(addr.set_host_port(IPv6, "ffaa::", 123));
        CHECK(addr.has_host_port());
        CHECK(addr.multicast());

        CHECK(!parse_socket_addr_miface("0.0.0.0", addr));
    }
}

} // namespace address
} // namespace roc
