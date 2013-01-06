/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
#define WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/connection_hdl.hpp>

#include <websocketpp/transport/asio/base.hpp>
#include <websocketpp/transport/base/connection.hpp>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace asio {

typedef lib::function<void(connection_hdl)> tcp_init_handler;

template <typename security>
class connection : public security {
public:
	typedef connection<security> type;
	typedef security security_type;
	
    typedef lib::shared_ptr<type> ptr;

	class handler_interface : public security_type::handler_interface {
	public:
		virtual void on_tcp_init() {};
	};
	
	typedef lib::shared_ptr<handler_interface> handler_ptr;
    typedef boost::asio::io_service* io_service_ptr;	

	// generate and manage our own io_service
	explicit connection(bool is_server)
	  : m_is_server(is_server)
	{
		std::cout << "asio connection transport constructor" << std::endl;
	}
	
	bool is_secure() const {
	    return security::is_secure();
	}
	
	/// Finish constructing the transport
	/**
	 * init_asio is called once immediately after construction to initialize
	 * boost::asio components to the io_service
	 *
	 * TODO: this method is not protected because the endpoint needs to call it.
	 * need to figure out if there is a way to friend the endpoint safely across
	 * different compilers.
	 */
    void init_asio (io_service_ptr io_service) {
    	// do we need to store or use the io_service at this level?
    	m_io_service = io_service;

        //m_strand.reset(new boost::asio::strand(*io_service));
    	
    	security::init_asio(io_service, m_is_server);
    }

    void set_tcp_init_handler(tcp_init_handler h) {
        m_tcp_init_handler = h;
    }

    /// Get the connection handle
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }
protected:
    /// Initialize transport for reading
	/**
	 * init_asio is called once immediately after construction to initialize
	 * boost::asio components to the io_service
	 */
    void init(init_handler callback) {
		std::cout << "asio connection init" << std::endl;
		
		security_type::init(
			lib::bind(
				&type::handle_init,
				this,
				callback,
				lib::placeholders::_1
			)
		);
	}
	
	void handle_init(init_handler callback, const lib::error_code& ec) {
		if (!ec) {
			m_handler->on_tcp_init();
		}
		
		callback(ec);
	}
    
    /// read at least num_bytes bytes into buf and then call handler. 
	/**
	 * 
	 * 
	 */
	void async_read_at_least(size_t num_bytes, char *buf, size_t len, 
		read_handler handler)
	{
		std::cout << "asio async_read_at_least: " 
		          << num_bytes << std::endl;
		
		if (num_bytes > len) {
			std::cout << "asio async_read_at_least error::invalid_num_bytes" << std::endl;
		    handler(make_error_code(error::invalid_num_bytes),size_t(0));
		    return;
		}
		
		boost::asio::async_read(
			security_type::get_socket(),
			boost::asio::buffer(buf,len),
			boost::asio::transfer_at_least(num_bytes),
			lib::bind(
				&type::handle_async_read,
				this,
				handler,
				lib::placeholders::_1,
				lib::placeholders::_2
			)
		);
	}
    
    void handle_async_read(read_handler handler, const 
        boost::system::error_code& ec, size_t bytes_transferred)
    {
    	// TODO: translate this better
    	if (ec) {
    		std::cout << "asio async_read_at_least error::pass_through" 
                      << "Original Error: " << ec 
                      << " (" << ec.message()  << ")"
                      << std::endl;
    		handler(make_error_code(error::pass_through), bytes_transferred);	
    	} else {
    		std::cout << "asio async_read_at_least no error" << std::endl;
    		handler(lib::error_code(), bytes_transferred);
    	}
    }
    
    void async_write(const char* buf, size_t len, write_handler handler) {
        m_bufs.push_back(boost::asio::buffer(buf,len));
    	
        boost::asio::async_write(
			security_type::get_socket(),
            m_bufs,
			lib::bind(
				&type::handle_async_write,
				this,
				handler,
				lib::placeholders::_1
			)
		);
    }
    
    void async_write(const std::vector<buffer>& bufs, write_handler handler) {
        std::vector<buffer>::const_iterator it;

        for (it = bufs.begin(); it != bufs.end(); ++it) {
            m_bufs.push_back(boost::asio::buffer((*it).buf,(*it).len));
        }
        
    	boost::asio::async_write(
			security_type::get_socket(),
			m_bufs,
			lib::bind(
				&type::handle_async_write,
				this,
				handler,
				lib::placeholders::_1
			)
		);
    }

    void handle_async_write(write_handler handler, const 
    	boost::system::error_code& ec)
    {
        m_bufs.clear();
    	// TODO: translate this better
    	if (ec) {
    		handler(make_error_code(error::pass_through));	
    	} else {
    		handler(lib::error_code());
    	}
    }
    
	void set_handler(handler_ptr new_handler) {
		m_handler = new_handler;
		
		security::set_handler(m_handler);
	}
    
    /// Set Connection Handle
    /**
     * See common/connection_hdl.hpp for information
     * 
     * @param hdl A connection_hdl that the transport will use to refer 
     * to itself
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
    }

    lib::error_code inturrupt(inturrupt_handler handler) {
        // strand post handle_inturrupt
        /*m_strand->post(lib::bind(
            &type::handle_inturrupt,
            this,
            handler
        ));*/

        // would this work?
        m_io_service->post(handler);
        return lib::error_code();
    }
    
    lib::error_code dispatch(dispatch_handler handler) {
        m_io_service->post(handler);
        return lib::error_code();
    }

    void handle_inturrupt(inturrupt_handler handler) { 
        handler();
    }
    
    /// close and clean up the underlying socket
    void shutdown() {
        security_type::shutdown();
    }
private:
	// static settings
	const bool			m_is_server;
	
	// dynamic settings
	
	// transport state
	
	// transport resources
    io_service_ptr      m_io_service;
	handler_ptr			m_handler;
    connection_hdl      m_connection_hdl;
    std::vector<boost::asio::const_buffer> m_bufs;

    // Handlers
    tcp_init_handler    m_tcp_init_handler;
};


} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
