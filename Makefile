CFLAGS = -O3 -D__STDC_CONSTANT_MACROS
LDFLAGS = -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lbz2 -lz -lc
INSTALLED_DEPS = -I/usr/local/include -L/usr/local/lib

mpegflow: mpegflow.cpp
	g++ $< -o $@ $(CFLAGS) $(LDFLAGS) $(INSTALLED_DEPS)

vis: vis.cpp
	g++ -std=c++11 $< -o $@ $(CFLAGS) -I/usr/local/Cellar/opencv/4.3.0/include/opencv4/opencv -I/usr/local/Cellar/opencv/4.3.0/include/opencv4 -L/usr/local/Cellar/opencv/4.3.0/lib -lopencv_highgui -lopencv_videoio -lopencv_imgproc -lopencv_imgcodecs -lopencv_core -lpng 

mpegflow.exe : mpegflow.cpp
	cl $? /MT /EHsc /I$(FFMPEG_DIR)\include /link avcodec.lib avformat.lib avutil.lib swscale.lib swresample.lib /LIBPATH:$(FFMPEG_DIR)\lib /OUT:$@
	for %I in ($(FFMPEG_DIR:dev=shared)\bin\avutil-*.dll $(FFMPEG_DIR:dev=shared)\bin\avformat-*.dll $(FFMPEG_DIR:dev=shared)\bin\avcodec-*.dll $(FFMPEG_DIR:dev=shared)\bin\swresample-*.dll) do copy %I $(MAKEDIR)

vis.exe: vis.cpp
	cl $? /MT /EHsc /I$(OPENCV_DIR)\..\..\include /link opencv_world320.lib /LIBPATH:$(OPENCV_DIR)\lib /OUT:$@
	for %I in ($(OPENCV_DIR)\bin\opencv_world320.dll $(OPENCV_DIR)\bin\opencv_world320_64.dll) do copy %I $(MAKEDIR)

clean:
	$(OS:Windows_NT=del) rm mpegflow vis *.exe *.obj *.dll
