This tool converts a log stream of binary packets to human-readable output.

Reuses shared packet definitions and utilities found in [`common/inc/`](../../common/inc/).

See the [`commander`](../commander) readme for a nicer way to launch multiple `monitor`s.

Launch with:
```bash
# One-time read to end of file
make && cat ../../common/tests/data.bin | ./monitor

# Continuously read growing file
make && tail -c +1 -f ../commander/all.bin | ./monitor
```

Example output:
```
Sequence 0, Origin 0 Internal, ID 2 ParsingErrorInvalidLength: 134678021
Sequence 0, Origin 0 Internal, ID 6 ParsingErrorDroppedBytes: 64
Sequence 1, Origin 0 Internal, ID 8 VfdSetFrequency: node 3, frequency 2.5 Hz
Sequence 2, Origin 0 Internal, ID 8 VfdSetFrequency: node 3, frequency 5.0 Hz
Sequence 0, Origin 0 Internal, ID 2 ParsingErrorInvalidLength: 1
Sequence 0, Origin 0 Internal, ID 2 ParsingErrorInvalidLength: 316
Sequence 0, Origin 0 Internal, ID 4 ParsingErrorInvalidID: 12
Sequence 0, Origin 0 Internal, ID 3 ParsingErrorInvalidCRC: provided 0x000004D2, calculated 0xDE4F523F
Sequence 0, Origin 0 Internal, ID 6 ParsingErrorDroppedBytes: 112
Sequence 0, Origin 0 Internal, ID 5 ParsingErrorInvalidSequence: provided 7, expected 3
Sequence 7, Origin 0 Internal, ID 8 VfdSetFrequency: node 3, frequency 5.0 Hz
Sequence 8, Origin 0 Internal, ID 8 VfdSetFrequency: node 3, frequency 5.0 Hz
Sequence 0, Origin 0 Internal, ID 6 ParsingErrorDroppedBytes: 7
Got EOF
read a total of 318 bytes
remaining bytes 23
done
```
