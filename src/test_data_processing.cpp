/*
 * Copyright 2017 Antonio Pastor anpastor{at}it.uc3m.es
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <random>
#include <unistd.h>
#include <iostream>     // std::cout
#include <vector>
#include <tuple>        // std::tuple, std::get, std::tie, std::ignore
#include <map>
#include <unordered_map>
#include <gflags/gflags.h>
#include <boost/thread.hpp>
#include <pqxx/pqxx>
#include <fstream>

#include "zmqpp/zmqpp.hpp"

//#include "../data-functions.h"

using std::cout;
using std::endl;
using std::string;

#define T_MSG   1 // microseconds
#define ONEEXPNINE 1000000000 //1e9

// TO-DO: define arguments for test running time and throughtput
DEFINE_string(dspIP, "*", "IP address of the DSP");
DEFINE_string(dbIP, "127.0.0.1", "IP address of the database");
DEFINE_int32(dbPORT, 5430, "Port of the database");
DEFINE_string(dbPWD, "", "password of the database");
DEFINE_string(dbUSER, "nameles", "database user");
DEFINE_string(dbNAME, "nameles", "database name");
DEFINE_string(domainsFile, "", "filename with list of domains for the test");
DEFINE_string(IPsFile, "", "filename with list of IPs for the test");
//DEFINE_int32(test_duration, 60, "Test duration in seconds");
DEFINE_int32(n_msgs, 30000, "Queries per second");
DEFINE_int32(nWorkers, 4, "Number of workers");
DEFINE_int32(max_msgs, 1e4, "Maximum number of messages per thread per database serialization");
DEFINE_int32(rcvport, 58505, "\"Receive from\" port");
DEFINE_int32(sndport, 58501, "\"Send to\" port");

typedef std::tuple<uint32_t,uint32_t,string> msg_tuple;
typedef std::vector<msg_tuple> msgs_list;

typedef std::pair<std::string, std::string> strings_pair;
struct StringPairHash {
 std::size_t operator()(const strings_pair& k) const
 {
     return std::hash<std::string>()(k.first) ^
            (std::hash<std::string>()(k.second) << 1);
 }};

struct StringPairEqual {
 bool operator()(const strings_pair& lhs, const strings_pair& rhs) const
 {
    return lhs.first == rhs.first && lhs.second == rhs.second;
 }
};

typedef std::unordered_map<strings_pair, unsigned int, StringPairHash, StringPairEqual> strings_pair_counter_map;

long time_diff(const timespec t0, const timespec t1);

void send_msgs_random(const string sendToSocket, const int n_msgs);
void receive_msgs(const string receiveFromSocket, const int n_msgs);

boost::thread senderThread, receiverThread;

void SIGINT_handler(int s){
					printf("Caught signal %d\n",s);
					receiverThread.interrupt();
					senderThread.interrupt();
}


int main (int argc, char *argv[]){
	google::ParseCommandLineFlags(&argc, &argv, true);

	string sendToSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_sndport);
	string receiveFromSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_rcvport);
	string dbConnect = "dbname=" + FLAGS_dbNAME + " user="+ FLAGS_dbUSER +
										" host="+ FLAGS_dbIP + " port=" + std::to_string(FLAGS_dbPORT) + " password=" + FLAGS_dbPWD;

	boost::thread senderThread(send_msgs_random, sendToSocket, FLAGS_n_msgs);

	receiverThread = boost::thread(receive_msgs, receiveFromSocket, FLAGS_n_msgs);
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = SIGINT_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	senderThread.join();
	sleep(5);
	kill(0, SIGINT);
	receiverThread.join();

  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  std::ostringstream date_oss;
  date_oss << std::put_time(&tm, "%C%m%d");

	pqxx::connection c(dbConnect);
	pqxx::read_transaction txn(c);
	pqxx::result r = txn.exec("SELECT referrer, host(ip), sum(cnt) AS cnt FROM tuples.temp_" + date_oss.str()
						+ " GROUP BY host(ip),referrer");

	std::ofstream file_db;
	file_db.open("db_counts.csv");
	for (auto row: r) {
		file_db << row[0].as<string>() << "," << row[1].as<string>() << "," << row[2].as<string>() << endl;
	}
	file_db.close();
	exit(0);
}


long time_diff(const timespec t0, const timespec t1){
	long sec = t1.tv_sec - t0.tv_sec;
	long nsec = t1.tv_nsec - t0.tv_nsec;
	if (nsec<0){
		nsec = ONEEXPNINE - nsec;
		sec--;
	}
	return sec*ONEEXPNINE + nsec;
}

void send_msgs_random(const string sendToSocket, const int n_msgs){
	zmqpp::context_t context;
	context.set(zmqpp::context_option::io_threads, 2);
	zmqpp::socket_t  sender(context, zmqpp::socket_type::push);
	sender.set(zmqpp::socket_option::linger, 1000); // 1 second
	try {
	sender.bind(sendToSocket);
	cout << "sender thread socket: " << sendToSocket << endl;
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		sender.close();
		context.terminate();
		return;
	}

	strings_pair_counter_map bidTuplesCounter;

	std::ifstream domains_file{FLAGS_domainsFile};
	std::ifstream ips_file{FLAGS_IPsFile};
	using istritr = std::istream_iterator<std::string>;
	std::vector<std::string> domains{istritr(domains_file), istritr()};
	std::vector<std::string> ips{istritr(ips_file), istritr()};

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dist_domains(0, domains.size()-1);
	std::uniform_int_distribution<> dist_ips(0, ips.size()-1);

	cout << "Input data vectors created" << endl;
	cout << "# of domains: " << domains.size() << endl;
	cout << "# of IPs: " << ips.size() << endl;
	int pending_msgs = n_msgs;
	uint32_t reqID = 1;
	string referrer, ip;
	struct timespec t0, t1;
	// struct timespec ts0, ts1;
	// timespec tspec_sleep = {0, 0};
	// int msg_time = 1e9/MPS; // In nanoseconds
	// int debug_print = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
	while (pending_msgs--){
		// clock_gettime(CLOCK_MONOTONIC_RAW, &ts0);
		zmqpp::message_t bid_req;
		referrer = domains[dist_domains(gen)];
		ip = ips[dist_ips(gen)];
		bid_req << reqID << referrer << ip;
//		bid_req << reqID;
//		bid_req << row[0].as<string>();
//		bid_req << row[1].as<string>();
		sender.send(bid_req, true);
		reqID++;
		bidTuplesCounter[strings_pair(referrer,ip)]++;
		// clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);
		// long t_sleep = msg_time-time_diff(ts0,ts1);
		// cout << t_sleep << endl;
		// if (t_sleep>0){
		// 	tspec_sleep.tv_nsec = t_sleep;
		// 	clock_nanosleep(CLOCK_MONOTONIC, 0, &tspec_sleep, NULL);
		// }

	}
	sender.close();
	context.terminate();

	std::vector<std::pair<strings_pair, unsigned int>> vector( bidTuplesCounter.begin(), bidTuplesCounter.end() );

	// Sort the vector according to the count in descending order.
	std::sort( vector.begin(), vector.end(),
						[]( const auto & lhs, const auto & rhs )
						{ return lhs.second > rhs.second; } );

	std::ofstream file_input;
	file_input.open("input_counts.csv");
	for ( const auto & it : vector){
		file_input << it.first.first << "," << it.first.second << "," << it.second << endl;
	}
	file_input.close();

	clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
	cout << "Test duration: " << time_diff(t0, t1)/1e9 << endl;
	cout << "# of messages sent: " << n_msgs << endl;
}


void receive_msgs(const string receiveFromSocket, const int n_msgs){

	zmqpp::context_t context;
	context.set(zmqpp::context_option::io_threads, 2);
	zmqpp::socket_t  receiver(context, zmqpp::socket_type::pull);
	receiver.set(zmqpp::socket_option::linger, 1000); // 1 second
	receiver.set(zmqpp::socket_option::receive_timeout, 10000); // 10 seconds
	try {
		receiver.bind(receiveFromSocket);
	} catch (zmqpp::zmq_internal_exception &e){
		cout << "Exception: " << e.what() << endl;
		receiver.close();
		context.terminate();
		return;
	}
	cout << "receiver thread socket: " << receiveFromSocket << endl;

	zmqpp::message_t reply;
	int count = 0;

	while( ! boost::this_thread::interruption_requested() ){
		if (receiver.receive(reply)) {
			count++;
		}
	}
	cout << "received " << count << " messages" << endl;

	receiver.close();
	context.terminate();
}
