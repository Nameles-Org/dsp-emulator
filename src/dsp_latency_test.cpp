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
#include <unistd.h>
#include <iostream>     // std::cout
#include <vector>
#include <tuple>        // std::tuple, std::get, std::tie, std::ignore
#include <unordered_map>
#include <gflags/gflags.h>
#include <boost/thread.hpp>
#include <pqxx/pqxx>

#include "zmqpp/zmqpp.hpp"

using std::cout;
using std::endl;
using std::string;

#define T_MSG   1 // microseconds
#define ONEEXPNINE 1000000000 //1e9

DEFINE_string(day, "161201", "Day of the database to use for the test queries (in format YYMMDD)");
DEFINE_string(dspIP, "*", "IP address of the DSP");
DEFINE_string(dbIP, "127.0.0.1", "IP address of the database (data processing module)");
DEFINE_int32(dbPORT, 5430, "Database port");
DEFINE_string(dbPWD, "password", "password of the database");
DEFINE_string(dbUSER, "user", "database user");
DEFINE_string(dbNAME, "nameles", "database name");
DEFINE_int32(test_duration, 60, "Test duration in seconds");
DEFINE_int32(MPS, 30000, "Queries per second");
DEFINE_int32(rcvport, 58505, "\"Receive from\" port");
DEFINE_int32(sndport, 58501, "\"Send to\" port");

typedef std::tuple<uint32_t,uint32_t,string> msg_tuple;
typedef std::vector<msg_tuple> msgs_list;


// GLOBAL VARIABLES
boost::thread receiverThread;
std::unordered_map<uint32_t, timespec> pending_requests;


void SIGINT_handler(int s){
           printf("Caught signal %d\n",s);
           receiverThread.interrupt();
}

void send_msgs(const string sendToSocket, const int n_msgs, const int MPS);

void receive_msgs(const string receiveFromSocket, const int n_msgs);

long time_diff(const timespec t0, const timespec t1);

int main (int argc, char *argv[]){
	google::ParseCommandLineFlags(&argc, &argv, true);

	int n_msgs = FLAGS_test_duration * FLAGS_MPS;
	pending_requests.reserve(n_msgs);
	string sendToSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_sndport);
	string receiveFromSocket = "tcp://" + FLAGS_dspIP + ":" + std::to_string(FLAGS_rcvport);

	boost::thread senderThread(send_msgs, sendToSocket, n_msgs, FLAGS_MPS);
	//usleep(5000);
	receiverThread = boost::thread(receive_msgs, receiveFromSocket, n_msgs);
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = SIGINT_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	senderThread.join();
	sleep(5);
	kill(0, SIGINT);
	receiverThread.join();
  return 0;
}

void send_msgs(const string sendToSocket, const int n_msgs, const int MPS){
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
	pqxx::connection c("dbname=" + FLAGS_dbNAME + " user="+ FLAGS_dbUSER +
                     " host="+ FLAGS_dbIP + " port=" + std::to_string(FLAGS_dbPORT) +
										 " password=" + FLAGS_dbPWD);
	pqxx::read_transaction txn(c);
	pqxx::result r = txn.exec("SELECT referrer, host(ip) FROM tuples.ip_ref_" + FLAGS_day + " LIMIT " + std::to_string(n_msgs) );
	txn.commit();
	// cout << "Test queries loaded, starting latency test" << endl;
	uint32_t reqID = 1;
	struct timespec t0, t1;
	struct timespec ts0, ts1;
	timespec tspec_sleep = {0, 0};
	int msg_time = 1e9/MPS; // In nanoseconds
	// int debug_print = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW, &t0);

	for (auto row: r) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts0);
		zmqpp::message_t bid_req;
		bid_req << reqID << row[0].as<string>() << row[1].as<string>();
//		bid_req << reqID;
//		bid_req << row[0].as<string>();
//		bid_req << row[1].as<string>();
		sender.send(bid_req, true);
		pending_requests[reqID] = ts0;
		reqID++;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts1);
		long t_sleep = msg_time-time_diff(ts0,ts1);
		// cout << t_sleep << endl;
		if (t_sleep>0){
			tspec_sleep.tv_nsec = t_sleep;
			clock_nanosleep(CLOCK_MONOTONIC, 0, &tspec_sleep, NULL);
		}
		// if (debug_print-->0){
		// 	cout << "msg_time=" << msg_time << endl;
		// 	cout << "time_diff=" << time_diff(ts0,ts1) << endl;
		// 	cout << "t_sleep=" << t_sleep << endl;
		// 	cout << endl;
		// }
	}
	sender.close();
	context.terminate();

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
	uint32_t reqID;
	int score, category;
	timespec t0, t1;
	double latency;

	std::vector<double> latencies;
	latencies.reserve(n_msgs);
	while( ! boost::this_thread::interruption_requested() ){
		if (receiver.receive(reply)){
			reply.get(reqID, 0);
			reply.get(score, 1);
			reply.get(category, 2);
			t0 = pending_requests[reqID];
			clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
			latency = time_diff(t0, t1)/1e6;
			latencies.push_back(latency);
			// cout << t0.tv_nsec << ' ' << t1.tv_nsec << ' ' << latency << endl;
			// cout << reqID << " - " << referrer.c_str() << " - " << ip << endl;
		}
		// else{
		// 	cout << "timeout " << boost::this_thread::interruption_requested() << endl;
		// }
	}
	cout << "received " << latencies.size() << " messages" << endl;
	std::sort(latencies.begin(),latencies.end());
	// cout << round(.999*latencies.size()) << endl;
	cout << "Max latency: " << latencies[latencies.size()-1] << endl;
	cout << "Percentile 99.9: " << latencies[round(.999*latencies.size())-1] << " ms" << endl;
	cout << "Percentile 99.0: " << latencies[round(.99*latencies.size())-1] << " ms" << endl;
	cout << "Percentile 90.0: " << latencies[round(.90*latencies.size())-1] << " ms" << endl;
	receiver.close();
	context.terminate();
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
