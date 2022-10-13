#include <chrono>
#include <iostream>
#include <thread>
#include <tuple>

#include <signal.h>
#include <net/ethernet.h>

#include "parser.hpp"
#include "hello.h"
#include "macros.hpp"
#include "sender.hpp"
#include "dispatcher.hpp"
#include "receiver.hpp"
#include "config.hpp"

#include "PacketQueue.hpp"
#include "Logger.hpp"

/*
 *	./run.sh --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/perfect-links.config

 *	../example/hosts:
 *		1 localhost 11001
 * 		2 localhost 11002
 *		3 localhost 11003
 *
 * ../example/output/1.output
 * 		empty file
 *
 * ../example/configs/perfect-links.config
 * 		10 2
 *
 * 	Raw run:
 * 		./bin/da_proc --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/perfect-links.config
 * 		valgrind --track-fds=yes --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/da_proc --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/perfect-links.config
 */

static std::string outputFile;
static Logger logger;

static void stop(int)
{
	// reset signal handlers to default
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);

	// immediately stop network packet processing
	std::cout << "Immediately stopping network packet processing.\n";

	// write/flush output file if necessary
	std::cout << "Writing output.\n";

	logger.write(outputFile);

	// exit directly from signal handler
	exit(0);
}

int main(int argc, char** argv)
{
	signal(SIGTERM, stop);
	signal(SIGINT, stop);

	// `true` means that a config file is required.
	// Call with `false` if no config file is necessary.
	bool requireConfig = true;

	Parser parser(argc, argv);
	parser.parse();

	hello();
	std::cout << std::endl;

	std::cout << "My PID: " << getpid() << "\n";
	std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
		<< getpid() << "` to stop processing packets\n\n";

	std::cout << "My ID: " << parser.id() << "\n\n";

	std::cout << "List of resolved hosts is:\n";
	std::cout << "==========================\n";
	auto hosts = parser.hosts();
	for (auto& host : hosts)
	{
		std::cout << host.id << "\n";
		std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
		std::cout << "Machine-readable IP: " << host.ip << "\n";
		std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
		std::cout << "Machine-readbale Port: " << host.port << "\n";
		std::cout << "\n";
	}
	std::cout << "\n";

	std::cout << "Path to output:\n";
	std::cout << "===============\n";
	std::cout << parser.outputPath() << "\n\n";

	std::cout << "Path to config:\n";
	std::cout << "===============\n";
	std::cout << parser.configPath() << "\n\n";

	std::cout << "Doing some initialization...\n\n";

	outputFile += parser.outputPath();
	std::vector<PacketQueue<char*>*> queues;
	uint32_t i, n_messages, recv_proc;
	std::tie(n_messages, recv_proc) = parser.getConfig();

	PerformanceConfig config;
	config.messagesPerPacket = MESSAGES_PER_PACKET;
	config.windowSize = WINDOW_SIZE > n_messages ? n_messages : WINDOW_SIZE;
	config.timeoutSec = TIMEOUT_SEC;
	config.timeoutNano = TIMEOUT_NANO;

	std::thread* receivers = new std::thread[hosts.size()];
	std::thread dispatcher, sender;

	for (i = 0; i < hosts.size(); i++)
	{
		queues.push_back(new PacketQueue<char*>());
	}

	std::cout << "Broadcasting and delivering messages...\n\n";

	/* host is a receiver */
	if (parser.id() == recv_proc)
	{
		dispatcher = std::thread{ dispatch, hosts[recv_proc - 1], std::ref(queues) };
		for (i = 0; i < hosts.size(); i++)
		{
			if (i == recv_proc - 1)
			{
				continue;
			}
			receivers[i] = std::thread{ receive, std::ref(logger),
					config.windowSize, std::ref(queues[i]), parser.id(), n_messages };
		}
	}

	/* host is a sender */
	else
	{
		sender = std::thread{ send_to_host, std::ref(logger), std::ref(config),
				parser.id(), hosts[recv_proc - 1], n_messages };
	}

	// After a process finishes broadcasting,
	// it waits forever for the delivery of messages.
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::hours(1));
	}

	std::cout << "Writing output.\n";
	logger.write(outputFile);
	return 0;
}
