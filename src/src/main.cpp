#include <chrono>
#include <iostream>
#include <thread>
#include <tuple>

#include <signal.h>
#include <net/ethernet.h>

#include "parser.hpp"
#include "hello.h"
#include "macros.hpp"
#include "config.hpp"
#include "threads.hpp"

#include "ReceiverWindow.hpp"
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
 *
 *  Stress test:
 * 		python3 stress.py -r ../template_cpp/run.sh -t perfect -l /media/dcl/shared/template_cpp/output -p 10 -m 100
 */

static std::vector<PacketQueue<char*>*> queues;
static std::string outputFile;
static Logger logger;
static size_t number_of_receivers;

static uint32_t i, role, numberOfMessagesToBeSent, receiverProcess;

static pthread_t* receivers;
static int** receiver_sockets;
static pthread_t dispatcher, sender;
static std::vector<struct ReceiverArgs> receiver_args;
static PerformanceConfig* config;

static void stop(int)
{
	// reset signal handlers to default
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);

	// immediately stop network packet processing
	std::cout << "Immediately stopping network packet processing.\n";
	switch (role)
	{
	case RECEIVER:
		pthread_kill(dispatcher, SIGUSR1);
		pthread_join(dispatcher, NULL);
		for (size_t i = 0; i < number_of_receivers; i++)
		{
			if (i == receiverProcess - 1) continue;
			pthread_kill(receivers[i], SIGUSR1);
		}
		for (size_t i = 0; i < number_of_receivers; i++)
		{
			if (i == receiverProcess - 1) continue;
			pthread_join(receivers[i], NULL);
			close(*receiver_sockets[i]);
			free(receiver_sockets[i]);
		}
		break;

	case SENDER:
		pthread_kill(sender, SIGUSR1);
		pthread_join(sender, NULL);
		break;

	default:
		fprintf(stderr, "role");
		break;
	}

	delete[] receivers;
	while (queues.size())
	{
		delete(queues.back());
		queues.pop_back();
	}

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

	Parser* parser = new Parser(argc, argv);
	parser->parse();

	hello();
	std::cout << std::endl;

	std::cout << "My PID: " << getpid() << "\n";
	std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
		<< getpid() << "` to stop processing packets\n\n";

	std::cout << "My ID: " << parser->id() << "\n\n";

	std::cout << "List of resolved hosts is:\n";
	std::cout << "==========================\n";
	auto hosts = parser->hosts();
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
	std::cout << parser->outputPath() << "\n\n";

	std::cout << "Path to config:\n";
	std::cout << "===============\n";
	std::cout << parser->configPath() << "\n\n";

	std::cout << "Doing some initialization...\n\n";

	outputFile += parser->outputPath();

	std::tie(numberOfMessagesToBeSent, receiverProcess) = parser->getConfig();
	config = new PerformanceConfig(numberOfMessagesToBeSent, sizeof(struct MessageSequence));
	config->print();

	number_of_receivers = hosts.size();
	receivers = new pthread_t[number_of_receivers];
	receiver_sockets = new int* [number_of_receivers];
	role = parser->id() == receiverProcess ? RECEIVER : SENDER;

	struct DispatcherArgs dispatcher_args = { config, hosts[receiverProcess - 1], std::ref(queues) };
	struct SenderArgs sender_args = { std::ref(logger), config, parser->id(),
		hosts[receiverProcess - 1], numberOfMessagesToBeSent };

	switch (role)
	{
	case RECEIVER:

		/* host is a receiver */

		for (i = 0; i < number_of_receivers; i++)
		{
			queues.push_back(new PacketQueue<char*>());
			receiver_sockets[i] = reinterpret_cast<int*>(malloc(sizeof(int)));
			receiver_args.push_back({ std::ref(logger), config, queues[i], parser->id(),
				numberOfMessagesToBeSent, receiver_sockets[i] });
		}

		pthread_create(&dispatcher, NULL, dispatch, reinterpret_cast<void*>(&dispatcher_args));
		for (i = 0; i < number_of_receivers; i++)
		{
			if (i == receiverProcess - 1)
			{
				continue;
			}

			pthread_create(&receivers[i], NULL, receive,
				reinterpret_cast<void*>(&receiver_args[i]));
		}
		break;
	case SENDER:
		/* host is a sender */
		pthread_create(&sender, NULL, send, reinterpret_cast<void*>(&sender_args));
		break;
	default:
		fprintf(stderr, "role");
		break;
	}

	delete(parser);

	std::cout << "Broadcasting and delivering messages...\n";

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
