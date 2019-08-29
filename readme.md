This is a gstreamer pipeline to record from a (tcp baseded) rtsp server.

The recorder has a start and stop signal so it can do many recordings.

The purpose of this program is to demonstrate how to restart a rtsp recorder in gstreamer. 

It is faulty and it exist to have a reference to share with people more gifted than me, to help find problems.

To use it follow these instructions.

Start one console and run the fake_rtsp_sever/fake_rtsp_server.sh (Linux binary no source available) It will setup a fake tcp rtsp server with a looping sequence.

You can test the fake_rtsp_server using ffplay

ffplay -rtsp_transport tcp rtsp://127.0.0.1:8554/

When the rtsp server is running the recorder can be built and tested against it.

rm -rf outbox && make clean && make && make test

use keys 

1: to start recording
2: stop recording
q: quit
