#!/bin/bash

PORT1=1179
PORT2=2229
PORT3=3339

echo "🧹 Cleaning old coverage files and binaries..."
rm -f *.gcda *.gcno *.gcov server client

echo "🔧 Compiling with coverage flags..."
gcc -o server atom_warehouse.c -fprofile-arcs -ftest-coverage
gcc -o client atom_supplier.c -fprofile-arcs -ftest-coverage

echo "🚀 Starting server on port $PORT1..."
./server
./server $PORT1 &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ ERROR: Server failed to start"
    exit 1
fi

echo "✅ Server running (PID $SERVER_PID)"

echo ""
echo "🧪 Test 1: פקודות שגויות + כל המקרים שטרם כוסו"
./client 127.0.0.1 $PORT1 <<EOF
ADD
ADD 3
ADD CARBON
ADD HYDROGEN -5
ADD HYDROGEN 0
ADD HELIUM
ADD HELIUM 5
ADD CARBON 10
ADD HYDROGEN 3
ADD HELIUM 2
BADCOMMAND
EXIT
EOF


echo ""
echo "🧪 Test 2: Regular ADD command"
./client 127.0.0.1 $PORT1 <<EOF
ADD OXYGEN 2
EOF

echo ""
echo "🧪 Test 3: Regular negitiv numbar"
./client 127.0.0.1 $PORT1 <<EOF
ADD OXYGEN -1
ADD HELIUM 2
ADD OXYGEN 0
ADD CARBON 0
EOF

echo ""
echo "🧪 Test 3: Regular negitiv numbar"
./client 127.0.0.1 $PORT1 <<EOF
ADD CARBON 8
ADD HYDROGEN 6
EOF
echo ""
echo "🧪 Test 4:EXIT"
./client
./client 127.0.0.1 $PORT1 <<EOF
EXIT
EOF

echo ""
echo "🧪 Test 5: Simulate Ctrl+C (SIGINT) with wake-up"
sleep 2
kill -SIGINT $SERVER_PID
# Send a dummy client to trigger select() and let the server exit
sleep 1
./client 127.0.0.1 $PORT1 <<EOF
EXIT
EOF
wait $SERVER_PID

echo ""
echo "🚀 Starting server again on port $PORT2..."
./server $PORT2 &
SERVER2_PID=$!
sleep 1

echo "🧪 Test 6: Client sends SHUTDOWN"
./client 127.0.0.1 $PORT2 <<EOF
ADD OXYGEN 2
SHUTDOWN
EOF
wait $SERVER2_PID

echo ""
echo "🚀 Starting server again on port $PORT3..."
./server $PORT3 &
SERVER3_PID=$!
sleep 1

echo "🧪 Test 6: Client sends EXIT"
./client 127.0.0.1 $PORT3 <<EOF
ADD OXYGEN 2
EXIT
EOF


echo ""
echo "📊 Generating coverage reports..."
gcov  server-atom_warehouse.c
gcov  client-atom_supplier.c 

echo ""
echo "✅ Done."
