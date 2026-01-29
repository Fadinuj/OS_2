#!/bin/bash

TCP_PORT=4423
UDP_PORT=2278

echo "🧹 Cleaning old coverage files and binaries..."
rm -f *.gcda *.gcno *.gcov drinks_bar atom_supplier molecule_requester
rm -f /tmp/TCP_stream /tmp/UDP_datagram /tmp/server_stream /tmp/server_datagram /tmp/server_socket
rm -f /tmp/stdin_commands.txt /tmp/test_state.txt

echo "🔧 Compiling all components with coverage..."
gcc -o drinks_bar drinks_bar.c -fprofile-arcs -ftest-coverage
gcc -o atom_supplier atom_supplier.c -fprofile-arcs -ftest-coverage
gcc -o molecule_requester molecule_requester.c -fprofile-arcs -ftest-coverage

echo "🚀 Starting server (TCP:$TCP_PORT, UDP:$UDP_PORT)..."
./drinks_bar -T $TCP_PORT -U $UDP_PORT &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ ERROR: Server failed to start"
    exit 1
fi

echo "✅ Server running (PID $SERVER_PID)"

echo "🧪 Test 1: ADD valid atoms (TCP)"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
ADD HYDROGEN 100
ADD OXYGEN 100
ADD CARBON 100
EOF

echo "🧪 Test 2: DELIVER valid molecules (UDP)"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER WATER 3
DELIVER CARBON DIOXIDE 2
DELIVER GLUCOSE 1
DELIVER ALCOHOL 1
EOF

echo "🧪 Test 3: DELIVER invalid molecule or syntax"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER UNKNOWNMOLECULE 1
DELIVER BAD INPUT
DELIVER
DELIVER WATER 0
EOF

echo "🧪 Test 4: DELIVER without enough atoms"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER GLUCOSE 100
EOF

echo "🧪 Test 5: TCP invalid commands"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
ADD HYDROGEN -4
ADD HELIUM 2
HELLO
EOF

echo "🧪 Test 6: Exit commands"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
EXIT
EOF

./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
EXIT
EOF

echo "🧪 Test 7: Shutdown server"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
SHUTDOWN
EOF

wait $SERVER_PID 2>/dev/null

echo "🧪 Test 8: UDS Stream and Datagram - CRITICAL FOR FULL COVERAGE"
./drinks_bar --stream-path /tmp/TCP_stream -d /tmp/UDP_datagram -o 100 -h 100 -c 100 &
UDS_SERVER_PID=$!
sleep 1

if [[ ! -S /tmp/TCP_stream ]] || [[ ! -S /tmp/UDP_datagram ]]; then
    echo "❌ ERROR: UDS sockets not created"
    kill $UDS_SERVER_PID 2>/dev/null
    exit 1
fi

echo "✅ UDS sockets created successfully"

echo "🧪 Adding MANY atoms via UDS stream for successful delivery..."
./atom_supplier -f /tmp/TCP_stream <<EOF
ADD CARBON 1000
ADD OXYGEN 1000
ADD HYDROGEN 1000
EOF

echo "🧪 Testing UDS datagram - SUCCESSFUL DELIVERIES"
./molecule_requester -f /tmp/UDP_datagram <<EOF
DELIVER WATER 5
DELIVER CARBON DIOXIDE 3
DELIVER ALCOHOL 2
DELIVER GLUCOSE 1
EOF

echo "🧪 Testing UDS datagram - FAILURE SCENARIOS"
./molecule_requester -f /tmp/UDP_datagram <<EOF
DELIVER UNKNOWN_MOLECULE 1
DELIVER WATER 0
DELIVER
DELIVER BAD SYNTAX HERE
DELIVER WATER 9999
EOF

echo "🧪 Test 9: Multiple UDS Stream Connections"
echo "🚀 Starting multiple UDS stream clients in background..."

for i in {1..5}; do
    (
        sleep $(echo "scale=2; $i * 0.3" | bc 2>/dev/null || echo "0.3")
        printf "ADD HYDROGEN 10\nADD OXYGEN 5\nEXIT\n" | ./atom_supplier -f /tmp/TCP_stream
    ) &
    eval "CLIENT${i}_PID=$!"
done

echo "⏳ Waiting for all rapid connections..."
sleep 3

for i in {1..5}; do
    eval "wait \$CLIENT${i}_PID 2>/dev/null"
done

echo "✅ Multiple UDS stream connections test completed"

echo "🧪 Test 10: Command line options and help"
./drinks_bar -h 2>/dev/null || true
./drinks_bar -z 2>/dev/null || true
./atom_supplier -h 2>/dev/null || true
./atom_supplier -z 2>/dev/null || true
./molecule_requester -h 2>/dev/null || true
./molecule_requester -z 2>/dev/null || true

echo "🧪 Test 11: Timeout functionality"
./drinks_bar -T 9999 -U 9998 -t 2 &
TIMEOUT_PID=$!
sleep 4
if kill -0 $TIMEOUT_PID 2>/dev/null; then
    kill $TIMEOUT_PID
    echo "❌ Timeout test failed"
else
    echo "✅ Timeout test passed"
fi

echo "🧪 Test 12: Save/Load state functionality"
./drinks_bar -T 7771 -U 2221 -F /tmp/test_state.txt -h 50 -o 30 -c 20 &
STATE_SERVER_PID=$!
sleep 1

./atom_supplier -h 127.0.0.1 -p 7771 <<EOF
ADD HYDROGEN 100
ADD OXYGEN 50
SHUTDOWN
EOF

wait $STATE_SERVER_PID 2>/dev/null

# Load state and verify
if [[ -f /tmp/test_state.txt ]]; then
    echo "✅ State file created successfully"
    cat /tmp/test_state.txt
    
    # Start server again to load state
    ./drinks_bar -T 6772 -U 2262 -F /tmp/test_state.txt &
    LOAD_SERVER_PID=$!
    sleep 1
    
    ./atom_supplier -h 127.0.0.1 -p 6772 <<EOF
SHUTDOWN
EOF
    
    wait $LOAD_SERVER_PID 2>/dev/null
    echo "✅ State load test completed"
else
    echo "❌ State file not created"
fi

# Close UDS server before stdin tests
echo "SHUTDOWN" | ./atom_supplier -f /tmp/TCP_stream
wait $UDS_SERVER_PID 2>/dev/null

echo ""
echo "🎯 ======================= CRITICAL STDIN TESTS (STAGE 6) ======================="
echo ""

echo "🧪 STDIN Test 1: Direct input with sufficient delay"
echo "Creating command file..."
cat > /tmp/stdin_commands.txt <<'EOF'
GEN SOFT
GEN VODKA
GEN CHAMPAGNE
GEN UNKNOWN
GEN
INVALID COMMAND
EOF

echo "🚀 Starting server with stdin input (Method 1)..."
(
    # Wait for server to fully start
    sleep 2
    
    # Send commands with delays
    echo "GEN SOFT"
    sleep 1
    echo "GEN VODKA"
    sleep 1
    echo "GEN CHAMPAGNE"
    sleep 1
    echo "GEN UNKNOWN"
    sleep 1
    echo "GEN"
    sleep 1
    echo "INVALID COMMAND"
    sleep 1
    echo "GEN SOFT_DRINK"
    sleep 1
    echo "GEN SOFTDRINK"
    sleep 1
    
) | timeout 15 ./drinks_bar -T 8001 -U 8002 -o 200 -h 200 -c 200 &

STDIN_PID1=$!
sleep 12
kill -INT $STDIN_PID1 2>/dev/null
wait $STDIN_PID1 2>/dev/null

echo ""
echo "🧪 STDIN Test 2: Using expect-like approach"
(
    timeout 10 ./drinks_bar -T 8003 -U 8004 -o 150 -h 150 -c 150 &
    SERVER_PID=$!
    
    # Give server time to start
    sleep 2
    
    # Send stdin commands to the server
    exec 3>&1
    exec 1>&3
    
    {
        echo "GEN SOFT"
        sleep 1
        echo "GEN VODKA" 
        sleep 1
        echo "GEN CHAMPAGNE"
        sleep 1
        echo "GEN UNKNOWN"
        sleep 1
        echo "GEN"
        sleep 1
        echo "INVALID"
        sleep 1
    } | timeout 8 ./drinks_bar -T 8005 -U 8006 -o 100 -h 100 -c 100
    
) 2>/dev/null &

STDIN_PID2=$!
sleep 10
kill -INT $STDIN_PID2 2>/dev/null
wait $STDIN_PID2 2>/dev/null

echo ""
echo "🧪 STDIN Test 3: Background server with foreground input"
./drinks_bar -T 8007 -U 8008 -o 100 -h 100 -c 100 &
BG_SERVER_PID=$!
sleep 2

# Send stdin to the background server via process substitution
{
    echo "GEN SOFT"
    sleep 1
    echo "GEN VODKA"
    sleep 1
    echo "GEN CHAMPAGNE" 
    sleep 1
    echo "GEN UNKNOWN"
    sleep 1
    echo "GEN"
    sleep 1
    echo "INVALID COMMAND"
    sleep 1
} > /proc/$BG_SERVER_PID/fd/0 2>/dev/null || {
    echo "⚠️  Cannot write to server stdin via /proc (not supported on this system)"
}

sleep 3
kill -INT $BG_SERVER_PID 2>/dev/null
wait $BG_SERVER_PID 2>/dev/null

echo ""
echo "🧪 STDIN Test 4: Named pipe approach"
PIPE_NAME="/tmp/stdin_pipe_$$"
mkfifo "$PIPE_NAME" 2>/dev/null || {
    echo "⚠️  Cannot create named pipe"
}

if [[ -p "$PIPE_NAME" ]]; then
    # Start server with named pipe as stdin
    ./drinks_bar -T 8009 -U 8010 -o 100 -h 100 -c 100 < "$PIPE_NAME" &
    PIPE_SERVER_PID=$!
    
    # Send commands to the pipe
    {
        sleep 2
        echo "GEN SOFT"
        echo "GEN VODKA"
        echo "GEN CHAMPAGNE"
        echo "GEN UNKNOWN"
        echo "GEN"
        echo "INVALID COMMAND"
        echo "GEN SOFT_DRINK"
        echo "GEN SOFTDRINK"
    } > "$PIPE_NAME" &
    
    PIPE_WRITER_PID=$!
    
    sleep 8
    kill -INT $PIPE_SERVER_PID 2>/dev/null
    kill $PIPE_WRITER_PID 2>/dev/null
    wait $PIPE_SERVER_PID 2>/dev/null
    wait $PIPE_WRITER_PID 2>/dev/null
    
    rm -f "$PIPE_NAME"
fi

echo ""
echo "🧪 STDIN Test 5: File redirection with delays"
cat > /tmp/delayed_commands.txt <<'EOF'
GEN SOFT
GEN VODKA
GEN CHAMPAGNE
GEN UNKNOWN
GEN
INVALID COMMAND
GEN SOFT_DRINK
GEN SOFTDRINK
EOF

(
    sleep 1
    while IFS= read -r line; do
        echo "$line"
        sleep 0.5
    done < /tmp/delayed_commands.txt
) | timeout 10 ./drinks_bar -T 8011 -U 8012 -o 100 -h 100 -c 100 &

DELAYED_PID=$!
sleep 8
kill -INT $DELAYED_PID 2>/dev/null
wait $DELAYED_PID 2>/dev/null

echo ""
echo "🧪 STDIN Test 6: Interactive simulation"
{
    echo "GEN SOFT"
    sleep 2
    echo "GEN VODKA"
    sleep 2
    echo "GEN CHAMPAGNE"
    sleep 2
    echo "GEN UNKNOWN"
    sleep 2
    echo "GEN"
    sleep 2
    echo "INVALID COMMAND"
    sleep 2
} | timeout 15 ./drinks_bar -T 8013 -U 8014 -o 100 -h 100 -c 100 &

INTERACTIVE_PID=$!
sleep 12
kill -INT $INTERACTIVE_PID 2>/dev/null
wait $INTERACTIVE_PID 2>/dev/null

echo ""
echo "🧪 STDIN Test 7: Final comprehensive test"
timeout 12 bash -c '
    {
        sleep 3
        printf "GEN SOFT\n"
        sleep 1
        printf "GEN VODKA\n"
        sleep 1
        printf "GEN CHAMPAGNE\n"
        sleep 1
        printf "GEN UNKNOWN\n"
        sleep 1
        printf "GEN\n"
        sleep 1
        printf "INVALID COMMAND\n"
        sleep 1
        printf "GEN SOFT_DRINK\n"
        sleep 1
        printf "GEN SOFTDRINK\n"
        sleep 1
    } | ./drinks_bar -T 8015 -U 8016 -o 100 -h 100 -c 100
' &

FINAL_PID=$!
sleep 10
kill -INT $FINAL_PID 2>/dev/null
wait $FINAL_PID 2>/dev/null

echo ""
echo "🎯 ========================== END STDIN TESTS =================================="
echo ""

echo "🧪 Test 13: Edge cases and final UDS tests"
./drinks_bar --stream-path /tmp/final_stream -d /tmp/final_datagram -o 200 -h 200 -c 200 &
FINAL_SERVER_PID=$!
sleep 1

./atom_supplier -f /tmp/final_stream <<EOF
ADD HYDROGEN 500
ADD OXYGEN 500
ADD CARBON 500
EOF

./molecule_requester -f /tmp/final_datagram <<EOF
DELIVER WATER 10
DELIVER CARBON DIOXIDE 5
DELIVER GLUCOSE 3
DELIVER ALCOHOL 2
DELIVER WATER 0
DELIVER UNKNOWN 1
DELIVER
EOF

echo "SHUTDOWN" | ./atom_supplier -f /tmp/final_stream
wait $FINAL_SERVER_PID 2>/dev/null

echo ""
echo "🧪 Test 14: getopt case -t (timeout flag coverage)"
./drinks_bar -T 9001 -U 9002 -t 2 &  
GETOPT_T_PID=$!
sleep 3
kill -INT $GETOPT_T_PID 2>/dev/null
wait $GETOPT_T_PID 2>/dev/null


echo ""
echo "🧪 Test 15: getopt case -f (UDS socket path flag coverage)"
./drinks_bar -f /tmp/server_socket -T 9011 -U 9012 -t 1 &  
GETOPT_F_PID=$!
sleep 2
kill -INT $GETOPT_F_PID 2>/dev/null
wait $GETOPT_F_PID 2>/dev/null

echo ""
echo "🧪 Final UDS Datagram Coverage Test"

./drinks_bar --stream-path /tmp/final_stream -d /tmp/final_datagram -o 200 -h 200 -c 200 & 
UDS_FINAL_PID=$!
sleep 1

if [[ ! -S /tmp/final_datagram ]]; then
    echo "❌ ERROR: UDS datagram socket not created"
    kill $UDS_FINAL_PID 2>/dev/null
    exit 1
fi

./atom_supplier -f /tmp/final_stream <<EOF
ADD HYDROGEN 500
ADD OXYGEN 500
ADD CARBON 500
EOF


./molecule_requester -f /tmp/final_datagram <<EOF
DELIVER WATER 5
DELIVER CARBON DIOXIDE 3
DELIVER GLUCOSE 1
DELIVER ALCOHOL 2
DELIVER UNKNOWN_MOLECULE 1
DELIVER
DELIVER WATER 0
DELIVER BAD SYNTAX HERE
DELIVER WATER 9999
EOF


echo "SHUTDOWN" | ./atom_supplier -f /tmp/final_stream
wait $UDS_FINAL_PID 2>/dev/null

echo "✅ UDS Datagram test completed"


./drinks_bar --stream-path /tmp/final_stream -d /tmp/final_datagram -F /tmp/state_uds.txt -o 500 -h 500 -c 500 &
SERVER_PID=$!
sleep 1

./atom_supplier -f /tmp/final_stream <<EOF
ADD HYDROGEN 500
ADD OXYGEN 500
ADD CARBON 500
EOF

./molecule_requester -f /tmp/final_datagram <<EOF
DELIVER WATER 2
EOF

echo "SHUTDOWN" | ./atom_supplier -f /tmp/final_stream
wait $SERVER_PID


./drinks_bar -T 8000 -U 8001 -F /tmp/state_udp.txt -o 500 -h 500 -c 500 &
SERVER_PID=$!
sleep 1

./atom_supplier -h 127.0.0.1 -p 8000 <<EOF
ADD HYDROGEN 500
ADD OXYGEN 500
ADD CARBON 500
EOF

./molecule_requester -h 127.0.0.1 -p 8001 <<EOF
DELIVER ALCOHOL 1
EOF

./atom_supplier -h 127.0.0.1 -p 8000 <<EOF
SHUTDOWN
EOF

wait $SERVER_PID

./drinks_bar -s /tmp/test_stream -o 100 -h 100 -c 100 &
SERVER_PID=$!
sleep 1

(
    echo "ADD HYDROGEN 42"
    sleep 5  
) | ./atom_supplier -f /tmp/test_stream &
CLIENT_PID=$!

sleep 2
echo "SHUTDOWN" | ./atom_supplier -f /tmp/test_stream

./drinks_bar -s /tmp/test_stream -o 100 -h 100 -c 100 &
SERVER_PID=$!
sleep 1

(
    echo "ADD HYDROGEN 42"
    sleep 5  
) | ./atom_supplier -f /tmp/test_stream &
CLIENT_PID=$!

sleep 2
echo "SHUTDOWN" | ./atom_supplier -f /tmp/test_stream

wait $SERVER_PID 2>/dev/null
wait $CLIENT_PID 2>/dev/null


echo ""
echo "📊 Generating coverage reports..."
gcov drinks_bar.c
gcov atom_supplier.c  
gcov molecule_requester.c

echo ""
echo "🧹 Cleaning up temporary files..."
rm -f /tmp/TCP_stream /tmp/UDP_datagram /tmp/server_stream /tmp/server_datagram /tmp/server_socket
rm -f /tmp/edge_datagram /tmp/multi_stream /tmp/multi_datagram /tmp/final_stream /tmp/final_datagram
rm -f /tmp/stdin_commands.txt /tmp/delayed_commands.txt /tmp/test_state.txt

echo ""
echo "✅ Complete coverage test finished!"
echo ""
