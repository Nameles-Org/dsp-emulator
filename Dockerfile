FROM apastor/debian-zmqpp:stretch

#ENV SND_PORT 58501
#ENV RCV_PORT 58505

WORKDIR /dsp-emulator

ADD . /dsp-emulator

RUN apt-get install -y libpqxx-dev libgflags-dev libboost-system-dev libboost-thread-dev

RUN make

#EXPOSE $RCV_PORT $SND_PORT

ENTRYPOINT /dsp-emulator/dsp_latency_test -dspIP '*' -rcvport $RCV_PORT -sndport $SND_PORT \
    -dbIP $DB_IP -dbPORT $DB_PORT -dbUSER $DB_USER -dbPWD $DB_PWD -dbNAME $DB_NAME -day $DB_DAY \
    -MPS $MPS -test_duration $TEST_TIME
