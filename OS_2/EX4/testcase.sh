#!/bin/bash

TCP_PORT=4467
UDP_PORT=2282

echo "🧹 Cleaning old coverage files and binaries..."
rm -f *.gcda *.gcno *.gcov drinks_bar atom_supplier molecule_requester

echo "🔧 Compiling all components with coverage..."
gcc -o drinks_bar drinks_bar.c -fprofile-arcs -ftest-coverage
gcc -o atom_supplier atom_supplier.c -fprofile-arcs -ftest-coverage
gcc -o molecule_requester molecule_requester.c -fprofile-arcs -ftest-coverage

echo "🚀 Starting server (TCP:$TCP_PORT, UDP:$UDP_PORT)..."
./drinks_bar
./drinks_bar -T 1234 -U 5678 -z
./drinks_bar -T $TCP_PORT -U $UDP_PORT &
SERVER_PID=$!
sleep 1

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "❌ ERROR: Server failed to start"
    exit 1
fi

echo "✅ Server running (PID $SERVER_PID)"
echo ""

echo "🧪 Test 1: ADD valid atoms (TCP)"
./atom_supplier
./atom_supplier -h 127.0.0.1
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
ADD HYDROGEN 100
ADD OXYGEN 100
ADD CARBON 100
EOF

echo ""
echo "🧪 Test 2: DELIVER valid molecules (UDP)"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER WATER 3
DELIVER CARBON DIOXIDE 2
DELIVER GLUCOSE 1
DELIVER ALCOHOL 1
EOF

echo ""
echo "🧪 Test 3: DELIVER invalid molecule or syntax"
./molecule_requester
./molecule_requester -z 127.0.0.1
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER UNKNOWNMOLECULE 1
DELIVER BAD INPUT
DELIVER
EOF

echo ""
echo "🧪 Test 4: DELIVER without enough atoms"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
DELIVER GLUCOSE 100
EOF

echo ""
echo "🧪 Test 5: TCP invalid commands"
./atom_supplier
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
ADD HYDROGEN -4
ADD HELIUM 2
HELLO
EOF

echo ""
echo "🧪 Test 6: GEN soft and alcoholic drinks"
./molecule_requester -h 127.0.0.1 -p  $UDP_PORT <<EOF
GEN SOFTDRINK
GEN VODKA
GEN MOJITO
GEN COCKTAIL
EOF
echo ""
echo "🧪 Test 6.a: GEN soft and alcoholic drinks"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
GEN SOFT DRINK
GEN MOJITO
EOF

echo ""
echo "🧪 Test 6.b: GEN soft and alcoholic drinks"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
GEN CHAMPAGNE
GEN SOFT_DRINK
EOF

echo ""
echo "🧪 Test 7: Invalid GEN commands"
./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
GEN UNKNOWNDRINK
GEN
EOF

echo ""
echo "🧪 Test 8: EXIT and SHUTDOWN"
./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
EXIT
EOF

./molecule_requester -h 127.0.0.1 -p $UDP_PORT <<EOF
EXIT
EOF

./atom_supplier -h 127.0.0.1 -p $TCP_PORT <<EOF
SHUTDOWN
EOF

wait $SERVER_PID

echo ""
echo "🧪 Test 9: Client 1 stays connected, Client 2 shuts down the server"
./drinks_bar -T 8881 -U 3331 &

(
echo "ADD OXYGEN 10"
sleep 5
echo "EXIT"
) | ./atom_supplier -h 127.0.0.1  -p 8881 &

CLIENT1_PID=$!
sleep 1  


./atom_supplier  -h 127.0.0.1  -p 8881 <<EOF
SHUTDOWN
EOF


wait $CLIENT1_PID



echo ""
echo "🧪 Test 10: DELIVER molecule with space (parsed == 4)"
./drinks_bar -T 1986 -U 8786 &



echo "🧪 Test 10.a: ADD valid atoms (TCP)"
./atom_supplier -h 127.0.0.1 -p 1986 <<EOF
ADD HYDROGEN 100
ADD OXYGEN 100
ADD CARBON 100
EOF


./molecule_requester -h 127.0.0.1 -p 8786 <<EOF
DELIVER CARBON DIOXIDE 2
DELIVER CARBON_DIOXIDE 1
DELIVER FANCY JUICE 3
EOF

echo ""
echo "🧪 Test 11: GEN drinks via stdin (manual input)"
# שלח פקודות GEN ל־drinks_bar דרך stdin (pipe)
(
echo "GEN SOFT DRINK"
echo "GEN VODKA"
echo "GEN CHAMPAGNE"
sleep 1
echo "GEN UNKNOWN"
) | ./drinks_bar -T 1844 -U 3124 &
MANUAL_PID=$!
sleep 2


(
echo "GET SOFT DRINK"
echo "GEN UNKNOWN"
) | ./drinks_bar -T 1374 -U 2734 &
MANUAL_PID=$!
sleep 2

echo ""
echo "🧪 Test 11: GEN drinks via stdin (FULL)"
(
sleep 1
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
) | ./drinks_bar -T 6579 -U 9629 &
MANUAL_PID=$!
sleep 8
kill -INT $MANUAL_PID
wait $MANUAL_PID

echo "" | ./drinks_bar -T 1479 -U 7849 -t 10 &
PID=$!
sleep 12

if kill -0 $PID 2>/dev/null; then
    echo "❌ FAILED: drinks_bar did not exit after timeout"
    kill $PID
else
    echo "✅ PASSED: drinks_bar exited on timeout"
fi

echo "🧪 Test 11.a: cheek option"
(
sleep 1
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
) | ./drinks_bar -T 4478 -U 4178 -o 100 -h 100 -c 100 -t 90 &
MANUAL_PID=$!
sleep 8
kill -INT $MANUAL_PID
wait $MANUAL_PID


echo "🧪 Test Early Timeout: Quick timeout test"  
./drinks_bar -T 9878 -U 9697 -t 1 &
QUICK_PID=$!
sleep 3
if kill -0 $QUICK_PID 2>/dev/null; then
    kill $QUICK_PID
fi
wait $QUICK_PID 2>/dev/null

echo ""
echo "📊 Generating coverage reports..."
gcov drinks_bar.c
gcov atom_supplier.c
gcov molecule_requester.c

echo ""
echo "✅ Done."
