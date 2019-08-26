CFLAGS += `pkg-config --cflags --libs gstreamer-1.0 gstreamer-base-1.0`

.PHONY=test


make: main.c
	$(CC) -o main main.c recorder.c util.c $(CFLAGS)

test:
	mkdir -p outbox
	./rtsp_recorder rtsp://127.0.0.1:8554/

clean:
	rm -f rtsp_recorder *.ts *.m3u8
