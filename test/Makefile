CFLAGS = -O3
LDFLAGS = -pthread

all: cpp-test throughput

clean:
	-rm cpp-test throughput

*.o: ../tina.h ../tina_jobs.h

cpp-test: cpp-test.o
throughput: throughput.o
