#!/bin/bash

TCP_PORT=4444
UDP_PORT=5555

echo "🧹 Cleaning old coverage files and binaries..."
rm -f *.gcda *.gcno *.gcov server atom_client molecule_client

echo "🔧 Compiling all components with coverage..."
gcc -o server molecule_supplier.c -fprofile-arcs -ftest-coverage
gcc -o atom_client atom_supplier.c -fprofile-arcs -ftest-coverage
gcc -o molecule_client molecule_requester.c -fprofile-arcs -ftest-coverage

echo "🚀 Starting molecule server (TCP:$TCP_PORT, UDP:$UDP_PORT)..."
./server
./server $TCP_PORT $UDP_PORT &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ ERROR: Server failed to start"
    exit 1
fi

echo "✅ Server running (PID $SERVER_PID)"
echo ""

echo "🧪 Test 1: ADD valid atoms (TCP)"
./atom_client
./atom_client 127.0.0.1 $TCP_PORT <<EOF
ADD HYDROGEN 10
ADD OXYGEN 10
ADD CARBON 10
EOF

echo ""
echo "🧪 Test 2: DELIVER valid molecule (UDP)"
./molecule_requester
./molecule_client 127.0.0.1 $UDP_PORT <<EOF
DELIVER WATER 3
DELIVER CARBON_DIOXIDE 2
EOF

echo ""
echo "🧪 Test 3: DELIVER invalid molecule"
./molecule_client 127.0.0.1 $UDP_PORT <<EOF
DELIVER UNKNOWNMOLECULE 1
EOF

echo ""
echo "🧪 Test 4: DELIVER without enough atoms"
./molecule_client 127.0.0.1 $UDP_PORT <<EOF
DELIVER 
DELIVER WATER
DELIVER GLUCOSE 5
EOF

echo ""
echo "🧪 Test 5: TCP invalid commands"
./atom_client 127.0.0.1 $TCP_PORT <<EOF
ADD HYDROGEN -4
ADD HELIUM 2
HELLO
EOF

./atom_client 127.0.0.1 $TCP_PORT<<EOF
EXIT
EOF
./molecule_client 127.0.0.1 $UDP_PORT <<EOF
EXIT
EOF
echo ""
echo "🧪 Test 6: Send SHUTDOWN"
./atom_client 127.0.0.1 $TCP_PORT <<EOF
SHUTDOWN
EOF

wait $SERVER_PID
echo ""
echo "🌀 Restarting server for more tests..."
./server 6666 7777 &
SERVER_PID=$!
sleep 1

echo ""
echo "🧪 Test 7: Refill atoms and DELIVER again"
./atom_client 127.0.0.1 6666 <<EOF
ADD HYDROGEN 100
ADD OXYGEN 100
ADD CARBON 100
EOF

./molecule_client 127.0.0.1 7777 <<EOF
DELIVER CARBON_DIOXIDE 1
DELIVER WATER 1
DELIVER ALCOHOL 1
DELIVER UNKNOWN MOLECULE 1
DELIVER GLUCOSE 1
DELIVER CARBON_DIOXIDE 100
DELIVER WATER 1
DELIVER VODKA 2
DELIVER ALCOHOL 1
DELIVER CARBON DIOXIDE 1
EOF

./molecule_client 127.0.0.1 7777 <<EOF
DELIVER UNKNOWN MOLECULE 1
DELIVER SODIUM 1
EOF

echo ""
echo "🧪 Test 8: Client 1 stays connected, Client 2 shuts down the server"
(echo "ADD OXYGEN 10"; sleep 5; echo "EXIT") | ./atom_client 127.0.0.1 6666 &
CLIENT1_PID=$!
sleep 1

./atom_client 127.0.0.1 6666 <<EOF
SHUTDOWN
EOF

wait $CLIENT1_PID
wait $SERVER_PID

echo ""
echo "🌀 Restarting server for more tests..."
./server 8888 9999 &
SERVER_PID=$!
sleep 1

./atom_client 127.0.0.1 8888 <<EOF
EXIT
EOF

./molecule_client 127.0.0.1 9999<EOF
EXIT
EOF

./atom_client 127.0.0.1 8888 <<EOF
ADD CARBON 10
kill -SIGINT $SERVER_PID
EOF

./atom_client 127.0.0.1 8888 <<EOF
EXIT
EOF

echo ""
echo "🧪 Send SIGINT and wake server"
./server 3344 8844 &  # פורט זמני
SERVER_TMP=$!
sleep 2
kill -SIGINT $SERVER_TMP
sleep 1
./atom_client 127.0.0.1 3344 <<EOF
EXIT
EOF
wait $SERVER_TMP


echo ""
echo "📊 Generating coverage reports..."
gcov server-molecule_supplier.c
gcov atom_client-atom_supplier.c
gcov molecule_client-molecule_requester.c

echo ""
echo "✅ Done."
