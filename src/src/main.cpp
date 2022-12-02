#include <chrono>
#include <iostream>
#include <thread>
#include <tuple>
#include <atomic>
#include <map>
#include <set>

#include <signal.h>
#include <net/ethernet.h>

#include "parser.hpp"
#include "hello.h"
#include "macros.hpp"
#include "config.hpp"
#include "worker.hpp"
#include "network_utils.hpp"

#include "Barrier.hpp"
#include "ReceiverWindow.hpp"
#include "Logger.hpp"
#include "ConcurrentMap.hpp"

/*
 *	./run.sh --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/fifo-broadcast.config
 *	./run.sh --id 2 --hosts ../example/hosts --output ../example/output/2.output ../example/configs/fifo-broadcast.config
 *	./run.sh --id 3 --hosts ../example/hosts --output ../example/output/3.output ../example/configs/fifo-broadcast.config
 *	./run.sh --id 4 --hosts ../example/hosts --output ../example/output/4.output ../example/configs/fifo-broadcast.config

 *	../example/hosts:
 *		1 localhost 11001
 * 		2 localhost 11002
 *		3 localhost 11003
 *
 * ../example/output/1.output
 * 		empty file
 *
 * ../example/configs/fifo-broadcast.config
 * 		10
 *
 * 	Raw run:
 * 		./bin/da_proc --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/fifo-broadcast.config
 * 		valgrind --track-fds=yes --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/da_proc --id 1 --hosts ../example/hosts --output ../example/output/1.output ../example/configs/perfect-links.config
 *
 *  Stress test:
 * 		python3 stress.py -r ../template_cpp/run.sh -t fifo -l /media/dcl/shared/template_cpp/output -p 10 -m 100
 */

static std::string outputFile;
static Parser::Host host;
static Logger logger;
static std::vector<Parser::Host> hosts;

static Barrier* barrier;

static uint32_t i, n_messages;
static uint64_t id, network_size;
static pthread_t threads[MAX_THREADS];
static int rc, sockfd, epollfd;
static PerformanceConfig* config;

static struct SenderConnection** sender_connections;
static ConcurrentMap<int, SenderConnection*>* sender_timer_map;

static struct ReceiverConnection** receiver_connections;

static struct DeliveryConnection** delivery_connections;
static ConcurrentMap<int, DeliveryConnection*>* delivery_timer_map;

static socklen_t addrlen = sizeof(sockaddr);
static struct sockaddr_in server;
static struct epoll_event events, setup;

static BroadcastLogLock broadcast_log_lock;
static pthread_mutex_t epoll_event_lock;

static void stop(int)
{
	// reset signal handlers to default
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);

	// immediately stop network packet processing
	std::cout << "Immediately stopping network packet processing.\n";

	for (i = 0; i < MAX_THREADS; i++)
	{
		pthread_kill(threads[i], SIGUSR1);
	}
	for (i = 0; i < MAX_THREADS; i++)
	{
		pthread_join(threads[i], NULL);
	}

	// write/flush output file if necessary
	std::cout << "Writing output.\n";
	logger.write(outputFile);

	for (i = 0; i < network_size; i++)
	{
		delete sender_connections[i]->timer;
		delete sender_connections[i]->window;	// this crashes the process on the delivery server
		free(sender_connections[i]);

		delete receiver_connections[i]->window;	// this crashes the process on the delivery server
		free(receiver_connections[i]);

		delete delivery_connections[i]->timer;
		delete delivery_connections[i]->window;	// this crashes the process on the delivery server
		free(delivery_connections[i]);
	}

	hosts.clear();
	delete sender_timer_map;
	delete delivery_timer_map;
	delete config;
	delete barrier;
	delete[] sender_connections;
	delete[] receiver_connections;
	delete[] delivery_connections;

	pthread_mutex_destroy(&broadcast_log_lock.mutex);
	pthread_mutex_destroy(&epoll_event_lock);

	close(sockfd);
	close(epollfd);

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
	hosts = parser->hosts();
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

	n_messages = parser->getConfig();

	network_size = hosts.size();
	config = new PerformanceConfig(n_messages, sizeof(struct MessageSequence), network_size);
	config->print();
	id = parser->id();
	host = hosts[parser->id() - 1];
	setuphost(server, host.ip, host.port);

	sender_timer_map = new ConcurrentMap<int, SenderConnection*>();
	delivery_timer_map = new ConcurrentMap<int, DeliveryConnection*>();
	pthread_mutex_init(&broadcast_log_lock.mutex, NULL);
	pthread_mutex_init(&epoll_event_lock, NULL);

	sender_connections = new struct SenderConnection* [network_size];
	receiver_connections = new struct ReceiverConnection* [network_size];
	delivery_connections = new struct DeliveryConnection* [network_size];

	barrier = new Barrier(MAX_THREADS);

	for (i = 0; i < network_size; i++)
	{
		sender_connections[i] = reinterpret_cast<struct SenderConnection*>
			(malloc(sizeof(struct SenderConnection)));
		receiver_connections[i] = reinterpret_cast<struct ReceiverConnection*>
			(malloc(sizeof(struct ReceiverConnection)));
		delivery_connections[i] = reinterpret_cast<struct DeliveryConnection*>
			(malloc(sizeof(struct DeliveryConnection)));
	}

	std::cout << "Receiving on " << host.ipReadable() << ":" << host.portReadable() << "\n";

	sockfd = get_socket(config->getSocketBufferSize());
	if (sockfd == -1)
	{
		traceerror();
		return EXIT_FAILURE;
	}

	rc = bind(sockfd, reinterpret_cast<sockaddr*>(&server), sizeof(server));
	if (rc == -1)
	{
		perror("bind");
		traceerror();
		close(sockfd);
		return EXIT_FAILURE;
	}

	epollfd = epoll_setup(&setup);
	if (epollfd == -1)
	{
		traceerror();
		close(sockfd);
		return EXIT_FAILURE;
	}

	rc = epoll_add_fd(epollfd, sockfd, &setup);
	if (rc == -1)
	{
		traceerror();
		close(sockfd);
		return EXIT_FAILURE;
	}

	bzero(&events, sizeof(struct epoll_event));
	bzero(&setup, sizeof(struct epoll_event));

	struct NetworkStruct network = {
		&hosts,
		sockfd,
		epollfd,
		events,
		setup,
	};

	struct ThreadArgs thread_args = {
		config,
		std::ref(logger),
		parser->id(),
		n_messages,
		&network,
		sender_connections,
		sender_timer_map,
		&broadcast_log_lock,
		receiver_connections,
		delivery_connections,
		delivery_timer_map,
		&epoll_event_lock,
		barrier,
	};

	std::cout << "Broadcasting and delivering messages...\n";

	/* increase number of sockets allowed to open because we open a lot of timer file	*/
	/* descriptors, more specifically 40 times the number of hosts of the network. 		*/
	/* With test cases with up to 128 hosts, we need 5 120 file descriptors just 		*/
	/* to use timers. 																	*/
	int status = system("ulimit -n 8192");
	if (status == -1)
	{
		trace();
		perror("system");
		return EXIT_FAILURE;
	}

	for (i = 0; i < MAX_THREADS; i++)
	{
		pthread_create(&threads[i], NULL, work, reinterpret_cast<void*>(&thread_args));
	}

	delete parser;

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
