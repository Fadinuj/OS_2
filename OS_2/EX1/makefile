CC = gcc
CFLAGS = -Wall -Wextra -g -fprofile-arcs -ftest-coverage
LDFLAGS = -fprofile-arcs -ftest-coverage

SUPPLIER_SRC = atom_supplier.c
WAREHOUSE_SRC = atom_warehouse.c

SUPPLIER_BIN = client
WAREHOUSE_BIN = server

.PHONY: all clean test coverage run-supplier run-server

all: $(SUPPLIER_BIN) $(WAREHOUSE_BIN)

$(SUPPLIER_BIN): $(SUPPLIER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(WAREHOUSE_BIN): $(WAREHOUSE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run-server:
	./$(WAREHOUSE_BIN) 12345

run-supplier:
	./$(SUPPLIER_BIN) 127.0.0.1 12345

test: all
	./testcase.sh

coverage:
	gcov $(WAREHOUSE_SRC)
	gcov $(SUPPLIER_SRC)

clean:
	rm -f $(SUPPLIER_BIN) $(WAREHOUSE_BIN) *.gcno *.gcda *.gcov
