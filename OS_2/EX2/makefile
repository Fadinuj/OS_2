CC = gcc
CFLAGS = -Wall -Wextra -g -fprofile-arcs -ftest-coverage
LDFLAGS = -fprofile-arcs -ftest-coverage

SUPPLIER_SRC = molecule_supplier.c
ATOM_CLIENT_SRC = atom_supplier.c
MOLECULE_CLIENT_SRC = molecule_requester.c

SUPPLIER_BIN = server
ATOM_CLIENT_BIN = atom_client
MOLECULE_CLIENT_BIN = molecule_client

.PHONY: all clean test coverage run-server run-clients

all: $(SUPPLIER_BIN) $(ATOM_CLIENT_BIN) $(MOLECULE_CLIENT_BIN)

$(SUPPLIER_BIN): $(SUPPLIER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(ATOM_CLIENT_BIN): $(ATOM_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(MOLECULE_CLIENT_BIN): $(MOLECULE_CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: all
	./testcase.sh

coverage:
	gcov server-molecule_supplier.c
	gcov atom_client-atom_supplier.c
	gcov molecule_client-molecule_requester.c

clean:
	rm -f $(SUPPLIER_BIN) $(ATOM_CLIENT_BIN) $(MOLECULE_CLIENT_BIN) *.gcno *.gcda *.gcov
