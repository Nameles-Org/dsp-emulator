# dsp-emulator module
This module emulates the function of the DSP end for testing nameles. Currently it implements a latency test, but could be easily modified to perform other tests as well.

## Running the latency test in docker-compose

The latency test can be easily run with any of the docker-compose configuration files included in the repository. Adjust the parameters of these configuration files to your scenario.

You can launch directly the three Nameles modules using the `nameles-docker-compose.yml` configuration file.

```bash
sudo docker-compose -f nameles-docker-compose.yml up
```

The [Nameles-streaming](https://github.com/Nameles-Org/Nameles-streaming) module takes a bit (usually less than 1 minute) to configure the database cluster. To avoid starting the test without having the service of this module running, you can configure the `DELAY_START` variable in the `nameles-docker-compose.yml` to delay the start of the dsp-emulator service the number of seconds needed to launch the Nameles-streaming service.


If you prefer to launch each Nameles module independently, the dsp-emulator latency can be run with the following command:

```bash
sudo docker-compose -f nameles-docker-compose.yml up dsp-latency-test
```

The docker images will be pulled automatically from docker hub.

In either case, you can check the proper working of the system with the summary of the test program and accessing to the Nameles database from the host of the Nameles-streaming docker service. There you can check the logging of the messages on a temporary table of the current day named `temp_<YYMMDD>`.

  ```bash
  psql -h 127.0.0.1 -p 5430 -U nameles
  ```

  Note that for this last step you need to have installed the postgreSQL client.
  In Debian/Ubuntu systems you need the postgresql-client package:
  ```bash
  sudo apt-get install postgresql-client
  ```

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

The latency test can be run using lists of domains and IPs in text files or extracting the information from a postgreSQL database built with Nameles.

If the names of the files with the lists of domains and IPs are provided the program generates each message using a random domain and IP. If no filenames are passed as arguments, the program tries to connect to the specified database in order to extract data for the test.

The program accepts the following command line parameters:

  - **-domainsFile** (filename with list of domains for the test) *type:* string, *default:* ""
  - **-IPsFile** (filename with list of IPs for the test) *type:* string, *default:* ""


  - **-dbIP** (IP address of the database, data processing module) *type:* string, *default:* "127.0.0.1"
  - **-dbNAME** (database name) *type:* string, *default:* "nameles"
  - **-dbPWD** (password of the database) *type:* string, *default:* "password"
  - **-dbUSER** (database user) *type:* string, *default:* "user"
  - **-day** (Day of the database to use for the test queries in format YYMMDD)  *type:* string *default:* "161201"


  - **-MPS** (Queries per second) *type:* int *default:* 30.000
  - **-dspIP** (IP address of the DSP) *type:* string, *default:* "*"
  - **-rcvport** ("Receive from" port) *type:* int, *default:* 58505
  - **-sndport** ("Send to" port) *type:* int, *default:* 58501
  - **-test_duration** (Test duration in seconds) *type:* int, *default:* 60

For running the program, you need to specify at least the text files for domains and IPs:

```bash
./dsp_latency_test -dbIP <127.0.0.1> -dbUSER <nameles> -dbNAME <nameles>
```

Or the database parameters:
```bash
./dsp_latency_test -dbIP <127.0.0.1> -dbUSER <nameles> -dbNAME <nameles>
```
