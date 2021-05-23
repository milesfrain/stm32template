This tool tests the usb loopback capabilities of the connected device. It sends packets at a configurable data rate and checks for losses.

Reuses shared packet definitions found in `common/inc/packets.h`.

Currently defaults to the following parameters:
* Device: `/dev/ttyACM0`
* Data rate: `11.52 KBps` (maximum at 115200 baud with start and stop bits).

Launch with:
```
make
./throughput
```

Note that the default launch command is equivalent to running with these arguments:
```
./throughput
./throughput -d /dev/ttyACM0 -b 11520
./throughput --device /dev/ttyACM0 --byterate 11520
```

Use the `--help` flag for more CLI info.

Example output (No drops. 11.47 KBps - 99.6% of ideal):
```
Sent 8065 packets
Got 8032 packets (last id 8031). Dropped 0. Pending 34. Bps total 11474, in interval 10400
Sent 8079 packets
Got 8044 packets (last id 8043). Dropped 0. Pending 36. Bps total 11470, in interval 9600
Sent 8094 packets
Got 8064 packets (last id 8063). Dropped 0. Pending 31. Bps total 11478, in interval 16000
Sent 8108 packets
Got 8076 packets (last id 8075). Dropped 0. Pending 33. Bps total 11475, in interval 9600
Sent 8123 packets
Got 8089 packets (last id 8088). Dropped 0. Pending 35. Bps total 11473, in interval 10400
Sent 8137 packets
Got 8102 packets (last id 8101). Dropped 0. Pending 36. Bps total 11471, in interval 10400
Sent 8151 packets
Got 8121 packets (last id 8120). Dropped 0. Pending 31. Bps total 11478, in interval 15200
Sent 8166 packets
Got 8134 packets (last id 8133). Dropped 0. Pending 33. Bps total 11476, in interval 10400
```
