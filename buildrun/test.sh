#!/bin/sh

# Test script for buildrun.

# Can be used with my original inotify-based implementation too,
# provided you pass --old on the command line.

rm -rf testctl
rm -f testlog

# The inotify-based version wouldn't cope if the control file didn't
# exist to begin with. Create an empty one.
if test "x--old" = "x$1"; then touch testctl; fi

testwrite() { # usage: testwrite <identifier> <exitstatus>
    buildrun -w testctl sh -c "echo write start $1 >> testlog; sleep 2; echo write end $1 returning $2 >> testlog; exit $2"
}
testread() { # usage: testread <identifier>
    echo read start $1 >> testlog
    buildrun -r testctl sh -c "echo read end $1 >> testlog"
}

# Test 1: start buildrun -w first, then buildrun -r. The former
# completes successfully, and the latter should then finish.
( testwrite 1 0 ) &
sleep 0.5
testread 1

# Test 2: same, but this time the first -w fails and we need a second
# one that succeeds.
( testwrite 2a 1; sleep 0.5; testwrite 2b 0 ) &
sleep 0.5
testread 2

# Test 3: this time we arrange a failure, and then start up a buildrun
# -r while no -w is running at all, demonstrating that it can block
# even when there's nothing to block on.
testwrite 3pre 1
( testread 3 ) &
sleep 0.5
testwrite 3 0
sleep 0.5

# Test 4: same, but again we have a failure before a success.
testwrite 4pre 1
( testread 4 ) &
sleep 0.5
testwrite 4a 1; sleep 0.5; testwrite 4b 0
sleep 0.5

# Test 5: now that no -w run is active and the last one was
# successful, do a -r run on its own and check that it returns success
# without blocking.
testread 5

if diff -u testout.txt testlog; then
    # Files match. Report success and clear up.
    echo "Tests passed."
    rm -rf testctl
    rm -f testlog
else
    # They don't match. Leave the temporary files lying around and
    # return failure.
    exit 1
fi
