CC = gcc
CFLAGS = -Wall -Wextra -w  -g -fprofile-arcs -ftest-coverage
LDFLAGS = -fprofile-arcs -ftest-coverage

SERVER_SRC = drinks_bar.c
TCP_CLIENT_SRC = atom_supplier.c
UDP_CLIENT_SRC = molecule_requester.c

SERVER_BIN = drinks_bar
TCP_CLIENT_BIN = atom_supplier
UDP_CLIENT_BIN = molecule_requester

.PHONY: all clean test coverage

all: $(SERVER_BIN) $(TCP_CLIENT_BIN) $(UDP_CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TCP_CLIENT_BIN): $(TCP_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(UDP_CLIENT_BIN): $(UDP_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	./testcase.sh

coverage:
	gcov $(SERVER_SRC)
	gcov $(TCP_CLIENT_SRC)
	gcov $(UDP_CLIENT_SRC)

clean:
	rm -f $(SERVER_BIN) $(TCP_CLIENT_BIN) $(UDP_CLIENT_BIN) *.gcno *.gcda *.gcov
