# dsp-emulator module
This module emulates the function of the DSP end for testing nameles. Currently it implements a latency test.

## Running the latency test in docker-compose

The latency test can be easily run with the docker-compose.yml configuration file included in the repository. Adjust the parameters of the configuration file to your scenario. You can launch directly the two modules needed for the test, the [scoring-module](https://github.com/Nameles-Org/scoring-module) and the dsp-emulator with:

```bash
sudo docker-compose -f nameles-docker-compose.yml up
```

Or, if you prefer to launch separately the [scoring-module](https://github.com/Nameles-Org/scoring-module), just the dsp-emulator latency test with:

```bash
sudo docker-compose -f nameles-docker-compose.yml up dsp-latency-test
```

The docker images will be pulled automatically from docker hub.

## Building from source

1. Install the dependencies:
  ```bash
  sudo apt-get install make g++ libgflags-dev libboost-thread-dev libpqxx-dev libzmq3-dev
  ```

2. In debian zmqpp is not available in the repositories. Install it from github:
  ```bash
  wget https://github.com/zeromq/zmqpp/archive/4.1.2.tar.gz
  tar zxvf 4.1.2.tar.gz && cd zmqpp-4.1.2
  make && sudo make install && ldconfig
  cd ..
```

3. Build the binaries with make:

  ```bash
  make
  ```

## Running the latency test

The latency test extracts the information from a postgreSQL database you can build from log files. The parameters for the database can be passed from command line.

The program accepts the following command line parameters:

  - -MPS (Queries per second) type: int32 default: 30000
  - -day (Day of the database to use for the test queries in format YYMMDD))
   type: string default: "161201"
  - -dbIP (IP address of the database, data processing module) type: string
   default: "127.0.0.1"
  - -dbNAME (database name) type: string default: "nameles"
  - -dbPWD (password of the database) type: string default: "password"
  - -dbUSER (database user) type: string default: "user"
  - -dspIP (IP address of the DSP) type: string default: "*"
  - -rcvport ("Receive from" port) type: int32 default: 58505
  - -sndport ("Send to" port) type: int32 default: 58501
  - -test_duration (Test duration in seconds) type: int32 default: 60

In order to run the program, you need to specify at least the database parameters:
```bash
./dsp_latency_test -dbIP <127.0.0.1> -dbUSER <user> -dbNAME <nameles>
```
