#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <winsock2.h>

#include <array>

#pragma comment(lib,"ws2_32.lib") 
#pragma warning(disable:4996)

constexpr auto PORT = 8888U;

struct udp_addr : sockaddr_in
{
	udp_addr()
	{
		memset(reinterpret_cast<char*>(this), 0, sizeof(udp_addr));

		sin_family = AF_INET;
	}

	udp_addr(unsigned long addr, unsigned port) :
		udp_addr()
	{
		sin_port = htons(port);
		sin_addr.s_addr = addr; // inet_addr(str_addr);
	}
};

struct winsock
{
	using message_buffer = std::array<char, 512U>;

	WSADATA ws;

	struct udp_socket
	{
		int socket_id { (int)INVALID_SOCKET };

		udp_socket(int id) :
			socket_id(id)
		{;}

		template<typename ...Args>
		udp_socket(Args...args) :
			socket_id(socket((args)...))
		{;}

		bool is_valid() const
		{
			return socket_id != INVALID_SOCKET && socket_id != SOCKET_ERROR;
		}

		~udp_socket()
		{
			if (is_valid())
			{
				closesocket(socket_id);
			}
		}
	};

	using shared_udp_socket = std::shared_ptr<udp_socket>;

	winsock()
	{
		const auto result = WSAStartup(MAKEWORD(2, 2), &ws);

		if (result)
		{
			std::cout << "WSAStartup initialization failed! Errno: " << WSAGetLastError() << std::endl;
			assert(false);
		}
	}

	~winsock()
	{
		WSACleanup();
	}

	shared_udp_socket create_udp_socket() const
	{
		auto client_socket = std::make_shared<udp_socket>(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (!client_socket->is_valid())
		{
			std::cout << "Could not create socket! Errno: " << WSAGetLastError() << std::endl;
			assert(false);
		}

		return client_socket;
	}

	auto vsend(shared_udp_socket socket, message_buffer& buffer, const udp_addr& addr) const
	{
		return sendto(socket->socket_id, buffer.data(), buffer.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(udp_addr));
	}

	auto vrecv(shared_udp_socket socket, message_buffer& buffer, udp_addr& addr) const
	{
		int slen = sizeof(udp_addr);

		return recvfrom(socket->socket_id, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&addr), &slen);
	}

	auto vbind(shared_udp_socket socket, const udp_addr& addr) const
	{
		return bind(socket->socket_id, reinterpret_cast<const sockaddr*>(&addr), sizeof(udp_addr));
	}
};


int fclient()
{
	winsock ws;

	auto socket = ws.create_udp_socket();

	udp_addr server_addr(inet_addr("127.0.0.1"), PORT);

	winsock::message_buffer buffer;

	while (true)
	{
		printf("Enter message: ");

		std::cin.getline(buffer.data(), buffer.size());

		if (ws.vsend(socket, buffer, server_addr) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code: %d", WSAGetLastError());
		}

		if (ws.vrecv(socket, buffer, server_addr) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code: %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}

		std::cout << buffer.data() << std::endl;
	}

	return 0;
}

int fserver()
{
	winsock ws;

	auto socket = ws.create_udp_socket();

	udp_addr server_addr(INADDR_ANY, PORT);

	if (ws.vbind(socket, server_addr) == SOCKET_ERROR)
	{
		printf("Bind failed with error code: %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	winsock::message_buffer buffer;

	while (true)
	{
		printf("Waiting for data...");
		fflush(stdout);

		udp_addr client_addr;
		
		if (ws.vrecv(socket, buffer, client_addr) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code: %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}

		// print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		printf("Data: %s\n", buffer.data());

		printf("Enter answer: ");
		std::cin.getline(buffer.data(), buffer.size());

		if (ws.vsend(socket, buffer, client_addr) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code: %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

int main()
{
	// std::thread client(fclient);
	std::thread server(fserver);

	server.detach();

	return fclient();
}