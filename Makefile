CFLAGS += `pkg-config --cflags --libs gstreamer-1.0 gstreamer-base-1.0`

.PHONY=test


make: main.c
	$(CC) -o rtsp_recorder main.c recorder.c util.c $(CFLAGS)

test:
	mkdir -p outbox
	./rtsp_recorder rtsp://127.0.0.1:8554/1

clean:
	rm -rf rtsp_recorder outbox
