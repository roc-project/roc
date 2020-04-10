/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <string.h>

#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_core/array.h"
#include "roc_core/heap_allocator.h"
#include "roc_core/shared_ptr.h"

#include "roc_sdp/session_description.h"
#include "roc_sdp/media_type.h"
#include "roc_sdp/media_proto.h"

namespace roc {
namespace sdp {

%%{
    machine parse_sdp;
    write data;
}%%

bool parse_sdp(const char* str, SessionDescription& result) {

    // for ragel
    const char* p = str;
    const char *pe = str + strlen(str);

    const char *eof = pe;
    int cs = 0;

    // for start_token
    const char* start_p = NULL;

    // parse result
    bool success = false;

    const char* start_p_origin_username = NULL;
    const char* end_p_origin_sess_id = NULL;
    const char* start_p_origin_nettype = NULL;
    const char* end_p_origin_addr = NULL;

    // Address type of the current address being parsed.
    address::AddrFamily cur_addrtype = address::Family_Unknown;

    %%{

        action start_token {
            start_p = p;
        }

        action set_origin_unicast_address {
            if(!result.set_origin_unicast_address(cur_addrtype, start_p, p - start_p)) {
                roc_log(LogError, "sdp: parse origin: invalid unicast address");
                result.clear();
                return false;
            }
        }

        action set_guid {
            if(!result.set_guid(start_p_origin_username, 
                                end_p_origin_sess_id,
                                start_p_origin_nettype,
                                end_p_origin_addr)) {
                roc_log(LogError, "sdp: parse guid: invalid origin field");
                result.clear();
                return false;
            }
        }

        action set_session_connection_data {
            if(!result.set_session_connection_data(
                    cur_addrtype,
                    start_p, 
                    p - start_p)) {
                        
                roc_log(LogError, "sdp: parse session connection data: invalid connection address");
                result.clear();
                return false;
            }
        }

        action create_media {
            if(!result.create_media_description()) {
                roc_log(LogError, "sdp: parse media: impossible to add a new media description");
                result.clear();
                return false;
            }
        }

        action set_media_port {

            char* end_p = NULL;
            long port = strtol(start_p, &end_p, 10);

            if (port == LONG_MAX || port == LONG_MIN || end_p != p) {
                roc_log(LogError, "sdp: parse media: invalid port");
                return false;
            }

            if (!result.last_media_description()->set_port((int)port)) {
                roc_log(LogError, "sdp: parse media: invalid port");
                return false;
            }
        }

        action set_media_nb_ports {

            char* end_p = NULL;
            long nb_ports = strtol(start_p, &end_p, 10);

            if (nb_ports == LONG_MAX || nb_ports == LONG_MIN || end_p != p) {
                roc_log(LogError, "sdp: parse media: invalid number of ports");
                return false;
            }

            if (!result.last_media_description()->set_nb_ports((int)nb_ports)) {
                roc_log(LogError, "sdp: parse media: invalid number of ports");
                return false;
            }
        }

        action add_media_fmt {
            if(!result.last_media_description()->add_fmt(start_p, p)) {
                roc_log(LogError, "sdp: parse media: invalid media format");
                result.clear();
                return false;
            }
        }

        action add_media_connection_data {
            if(!result.last_media_description()->add_connection_data(cur_addrtype,
                    start_p, 
                    p - start_p)) {

                roc_log(LogError, "sdp: parse media connection: invalid connection address");
                result.clear();
                return false;
            }
        }

        ##### USEFUL GRAMMAR #####
        # ABNF: 1*(VCHAR/%x80-FF) -> string of visible characters
        non_ws_string = [!-~]+;      

        # ABNF: %x21 / %x23-27 / %x2A-2B / %x2D-2E / %x30-39 / %x41-5A / %x5E-7E
        token_char = ([!#\$%&\'\*\+\-\.\^_\`\{\|\}~] | alnum);
        token = token_char+;

        ##### SDP ATTRIBUTES - NOT YET IMPLEMENTED #####
        # <payload type> <encoding name>/<clock rate> [/<encoding parameters>]
        # (dynamic payload ID: encoding name, sample rate, channel set)
        a_rtpmap = "a=rtpmap";

        a_recvonly = "a=recvonly"; # (session mode / direction)
        a_sendrecv = "a=sendrecv"; # (--//--)
        a_sendonly = "a=sendonly"; # (--//--)
        a_inactive = "a=inactive"; # (--//--)
        a_type = "a=type"; # (session type; defines default session mode if omitted)
        a_fmtp = "a=fmtp"; # (codec-specific parameters; we'll need it for Opus)
        a_fec_source_flow = "a=fec-source-flow"; # (FECFRAME; see RFC 6364)
        a_fec_repair_flow = "a=fec-repair-flow"; # (--//--)
        a_repair_window = "a=repair-window"; # (--//--)

        ##### SDP FIELDS #####
        version = digit+;

        # o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
        origin_username = non_ws_string >start_token %{ start_p_origin_username = start_p; };
        origin_sess_id = digit+ >start_token %{ end_p_origin_sess_id = p; };
        origin_nettype = "IN" >start_token %{ start_p_origin_nettype = start_p; };

        # Either IPv6 or IPv4
         origin_unicast_address = non_ws_string >start_token
            %{  end_p_origin_addr = p; } 
            %set_origin_unicast_address;

         origin_unicast_address_with_addrtype =  
            ( "IP4" %{ cur_addrtype = address::Family_IPv4; } 
            | "IP6" %{ cur_addrtype = address::Family_IPv6; } 
            ) ' '  origin_unicast_address;

        # action todo: sess-id should be unique for this username/host
        origin = origin_username ' ' origin_sess_id ' '  digit+ ' ' 
            origin_nettype ' ' origin_unicast_address_with_addrtype %set_guid;

        
        # In session-level: c=<nettype> <addrtype> <connection-address>
        session_connection_nettype = "IN";
        
        # Either IPv6 or IPv4/TTL
        session_connection_address = non_ws_string >start_token
            %set_session_connection_data;

        session_connection_with_addrtype =  
            ( "IP4" %{ cur_addrtype = address::Family_IPv4; } 
                ' ' session_connection_address '/' digit+
            | "IP6" %{ cur_addrtype = address::Family_IPv6; } 
                ' ' session_connection_address
            );

        session_connection_data = session_connection_nettype ' ' 
            session_connection_with_addrtype;
    
        # Each media description starts with an "m=" field and is terminated by
        # either the next "m=" field or by the end of the session description
        # m=<type> <port> <proto> <fmt>

        # Typically "audio", "video", "text", or "application"
        media_type = ("audio"  %{ result.last_media_description()->set_type(sdp::MediaType_Audio); } 
            | "video" %{ result.last_media_description()->set_type(sdp::MediaType_Video); }
            | "text" %{ result.last_media_description()->set_type(sdp::MediaType_Text); }
            | "application" %{ result.last_media_description()->set_type(sdp::MediaType_Application); }
        );
            
        
        # Typically "RTP/AVP".
        media_proto = "RTP/AVP"  %{ result.last_media_description()->set_proto(sdp::MediaProto_RTP_AVP); };

        media_port = digit+ >start_token %set_media_port;
        
        media_nb_ports = digit+ >start_token %set_media_port;

        # typically an RTP payload type for audio and video media
        media_fmt = token >start_token %add_media_fmt;

        media_description = (media_type ' ' media_port ('/' media_nb_ports)? ' ' media_proto (' ' media_fmt)+)
            >create_media;
        
        media_field = 'm='i media_description;

        # In media-level: c=<nettype> <addrtype> <connection-address>
        media_connection_nettype = "IN";
        
        # Either IPv6 or IPv4/TTL
        media_connection_address = non_ws_string >start_token 
            %add_media_connection_data;

        media_connection_with_addrtype =  
            ( "IP4" %{ cur_addrtype = address::Family_IPv4; } 
                ' ' media_connection_address '/' digit+
            | "IP6" %{ cur_addrtype = address::Family_IPv6; } 
                ' ' media_connection_address
            );

        media_connection_data = media_connection_nettype ' ' 
            media_connection_with_addrtype;

        media_connection_field = 'c='i media_connection_data;

        media_fields = ('\n' media_field ('\n' media_connection_field)*)+;

        sdp_description = 'v='i version
        '\n' 'o='i origin
        '\n' 'c='i session_connection_data
        media_fields;

        main := sdp_description
                %{ success = true; }
                ;

        write init;
        write exec;
    }%%

    if (!success) {
        roc_log(LogError, "UNSUCCESS");
        result.clear();
        return false;
    }

    return true;
}

} // namespace sdp
} // namespace roc
