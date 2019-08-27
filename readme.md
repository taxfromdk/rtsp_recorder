This is a gstreamer pipeline to record from a (tcp baseded) rtsp server.

The recorder has a start and stop signal so it can do many recordings.

The purpose of this program is to demonstrate how to restart a rtsp recorder in gstreamer.

To use it follow these instructions.

Start one console and run the fake_rtsp_sever. (binary no source available)

You can test the fake_rtsp_server using ffplay

ffplay -rtsp_transport tcp rtsp://127.0.0.1:8554/

When the rtsp server is running the recorder can be built and tested against it.

make clean && make && make test
