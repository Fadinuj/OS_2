#!/bin/bash

TCP_PORT=4418
UDP_PORT=2200

echo "🧹 Cleaning old coverage files and binaries..."
rm -f *.gcda *.gcno *.gcov drinks_bar atom_supplier molecule_requester
rm -f /tmp/TCP_stream /tmp/UDP_datagram /tmp/server_stream /tmp/server_datagram /tmp/server_socket

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

echo "🧪 Test 6: GEN drinks via stdin - CRITICAL FOR STDIN COVERAGE"
cat > /tmp/gen_commands.txt <<EOF
GEN SOFT
GEN VODKA
GEN CHAMPAGNE
GEN UNKNOWN
GEN
INVALID COMMAND
EOF

timeout 5 ./drinks_bar -T 1843 -U 3123 -o 100 -h 100 -c 100 < /tmp/gen_commands.txt &
GEN_PID=$!
sleep 3
kill -INT $GEN_PID 2>/dev/null
wait $GEN_PID 2>/dev/null


echo "🧪 Alternative stdin test with printf"
(
printf "GEN SOFT\n"
sleep 0.5
printf "GEN VODKA\n" 
sleep 0.5
printf "GEN CHAMPAGNE\n"
sleep 0.5
printf "GEN UNKNOWN\n"
sleep 0.5
printf "GEN\n"
sleep 0.5
printf "INVALID\n"
sleep 0.5
) | timeout 5 ./drinks_bar -T 1842 -U 3122 -o 100 -h 100 -c 100 &
ALT_PID=$!
sleep 3
kill -INT $ALT_PID 2>/dev/null
wait $ALT_PID 2>/dev/null

rm -f /tmp/gen_commands.txt

echo "🧪 Test 7: Command line options and help"
./drinks_bar -h 2>/dev/null || true
./drinks_bar -z 2>/dev/null || true
./atom_supplier -h 2>/dev/null || true
./atom_supplier -z 2>/dev/null || true
./molecule_requester -h 2>/dev/null || true
./molecule_requester -z 2>/dev/null || true

echo "🧪 Test 8: Exit commands"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
EXIT
EOF

./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
EXIT
EOF

echo "🧪 Test 9: Shutdown server"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
SHUTDOWN
EOF

wait $SERVER_PID 2>/dev/null

echo "🧪 Test 10: UDS Stream and Datagram - CRITICAL FOR FULL COVERAGE"

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

echo "🧪 Testing UDS datagram - SUCCESSFUL DELIVERIES (this will cover lines 538-549)"
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


echo "🧪 More UDS datagram tests"
./molecule_requester -f /tmp/UDP_datagram <<EOF
DELIVER CARBON_DIOXIDE 1
DELIVER FANCY JUICE 1
DELIVER WATER
DELIVER ALCOHOL EXTRA PARAMS
EOF

echo "🧪 Test 10.5: CRITICAL - Multiple UDS Stream Connections to cover lines 342-360"


echo "🚀 Starting multiple UDS stream clients in background..."


(
    sleep 2
    echo "ADD HYDROGEN 50" | ./atom_supplier -f /tmp/TCP_stream
    sleep 1
) &
CLIENT1_PID=$!


(
    sleep 2.1
    echo "ADD OXYGEN 30" | ./atom_supplier -f /tmp/TCP_stream
    sleep 1
) &
CLIENT2_PID=$!

(
    sleep 2.5
    echo "ADD CARBON 20" | ./atom_supplier -f /tmp/TCP_stream
    sleep 1
) &
CLIENT3_PID=$!


(
    sleep 3
    printf "ADD HYDROGEN 10\nEXIT\n" | ./atom_supplier -f /tmp/TCP_stream
) &
CLIENT4_PID=$!

echo "⏳ Waiting for multiple clients to connect and send data..."
sleep 4


wait $CLIENT1_PID 2>/dev/null
wait $CLIENT2_PID 2>/dev/null  
wait $CLIENT3_PID 2>/dev/null
wait $CLIENT4_PID 2>/dev/null

echo "✅ Multiple UDS stream connections test completed"

echo "🧪 Test 11: Timeout functionality"
./drinks_bar -T 9299 -U 9298 -t 2 &
TIMEOUT_PID=$!
sleep 4
if kill -0 $TIMEOUT_PID 2>/dev/null; then
    kill $TIMEOUT_PID
    echo "❌ Timeout test failed"
else
    echo "✅ Timeout test passed"
fi

echo "🧪 Test 12: Complex client scenarios"
./drinks_bar -T 7971 -U 2921 &
COMPLEX_SERVER_PID=$!
sleep 1

(
echo "ADD OXYGEN 10"
sleep 3
echo "EXIT"
) | ./atom_supplier -h 127.0.0.1 -p 7971 &
CLIENT1_PID=$!

sleep 1
# לקוח 2 שולח SHUTDOWN
./atom_supplier -h 127.0.0.1 -p 7971 <<EOF
SHUTDOWN
EOF

wait $CLIENT1_PID 2>/dev/null

echo "🧪 Test 13: All getopt options"
./drinks_bar -s /tmp/server_stream -d /tmp/server_datagram -f /tmp/server_socket -t 1 -o 50 -h 50 -c 50 &
GETOPT_PID=$!
sleep 2
kill $GETOPT_PID 2>/dev/null
wait $GETOPT_PID 2>/dev/null

# נקה UDS server
kill $UDS_SERVER_PID 2>/dev/null
wait $UDS_SERVER_PID 2>/dev/null

echo "🧪 Test 14: Edge cases and error handling"
# שרת חדש לבדיקות edge cases
./drinks_bar -T 8881 -U 8882 -d /tmp/edge_datagram &
EDGE_SERVER_PID=$!
sleep 1

# **קריטי**: הוסף אטומים לפני בקשת מולקולות
./atom_supplier -h 127.0.0.1 -p 8881 <<EOF
ADD HYDROGEN 500
ADD OXYGEN 500
ADD CARBON 500
EOF

# בדוק UDS datagram עם כל התרחישים האפשריים - כולל SUCCESSFUL deliveries
echo "🧪 UDS datagram - SUCCESSFUL deliveries to cover lines 538-549"
./molecule_requester -f /tmp/edge_datagram <<EOF
DELIVER WATER 10
DELIVER CARBON DIOXIDE 5
DELIVER GLUCOSE 3
DELIVER ALCOHOL 2
EOF

# ואז תרחישי כישלון
echo "🧪 UDS datagram - FAILURE scenarios"
./molecule_requester -f /tmp/edge_datagram <<EOF
DELIVER WATER 0
DELIVER UNKNOWN 1
DELIVER
DELIVER TOO MANY PARAMS HERE
DELIVER WATER
DELIVER WATER 999
EOF

# סגור שרת
./atom_supplier -h 127.0.0.1 -p 8881 <<EOF
SHUTDOWN
EOF

wait $EDGE_SERVER_PID 2>/dev/null

echo "🧪 Test 15: Final stdin GEN test - COMPREHENSIVE"
# צור קובץ עם פקודות מקיפות יותר
cat > /tmp/comprehensive_gen.txt <<EOF
GEN SOFT
GEN SOFTDRINK  
GEN SOFT_DRINK
GEN VODKA
GEN CHAMPAGNE
GEN UNKNOWN
GEN INVALID_DRINK
GEN
INVALID COMMAND
NOT_GEN SOMETHING
EOF

# הפעל עם קלט מהקובץ
timeout 8 ./drinks_bar -T 4555 -U 5556 -o 200 -h 200 -c 200 < /tmp/comprehensive_gen.txt &
FINAL_PID=$!
sleep 5
kill -INT $FINAL_PID 2>/dev/null
wait $FINAL_PID 2>/dev/null

rm -f /tmp/comprehensive_gen.txt

echo "🧪 Test 16: Advanced Multiple UDS Connections - Max Coverage"
# שרת חדש עם UDS stream
./drinks_bar -s /tmp/multi_stream -d /tmp/multi_datagram -o 200 -h 200 -c 200 &
MULTI_SERVER_PID=$!
sleep 1

echo "🚀 Creating rapid succession of UDS connections..."

# צור הרבה חיבורים במהירות לכסות את כל הענפים בקוד
for i in {1..8}; do
    (
        sleep $(echo "scale=2; $i * 0.2" | bc)
        printf "ADD HYDROGEN 10\nADD OXYGEN 5\nEXIT\n" | ./atom_supplier -f /tmp/multi_stream
    ) &
    MULTI_CLIENTS[$i]=$!
done

echo "⏳ Waiting for all rapid connections..."
sleep 3

# המתן לכל הלקוחות
for pid in "${MULTI_CLIENTS[@]}"; do
    wait $pid 2>/dev/null
done

# סגור השרת
echo "SHUTDOWN" | ./atom_supplier -f /tmp/multi_stream
wait $MULTI_SERVER_PID 2>/dev/null

echo "🧪 Test 20: Simple approach - slow input"
# פשוט נשלח קלט באטיות
(
    sleep 3  # תן לשרת זמן להתחיל
    printf "GEN SOFT\n"
    sleep 2
    printf "GEN VODKA\n"
    sleep 2  
    printf "GEN CHAMPAGNE\n"
    sleep 2
    printf "GEN UNKNOWN\n"
    sleep 2
    printf "GEN\n"
    sleep 2
    printf "INVALID COMMAND\n"
    sleep 2
) | ./drinks_bar -T 9101 -U 9102 -o 200 -h 200 -c 200 &
SLOW_PID=$!
sleep 15  # תן הרבה יותר זמן
kill -INT $SLOW_PID 2>/dev/null

echo "🧪 Test 21: Manual test hint"
echo "If stdin coverage is still missing, try running manually:"
echo "./drinks_bar -T 1234 -U 5678"
echo "Then type: GEN SOFT, GEN VODKA, GEN CHAMPAGNE, GEN UNKNOWN, GEN, INVALID"

echo "📊 Generating coverage reports..."
gcov drinks_bar.c
gcov atom_supplier.c  
gcov molecule_requester.c

echo ""
echo "🧹 Cleaning up temporary files..."
rm -f /tmp/TCP_stream /tmp/UDP_datagram /tmp/server_stream /tmp/server_datagram /tmp/server_socket /tmp/edge_datagram /tmp/multi_stream /tmp/multi_datagram

echo "✅ Complete coverage test finished!"