geodraw.exe: geodraw.c geo.c geo.h tpkapi.c tpkapi.h tpkapi_windows.c
	mingw32-gcc -Os -o geodraw.exe geodraw.c geo.c tpkapi.c -lgdi32 -lws2_32 -lopengl32
	mingw32-strip geodraw.exe

clean::
	rm -f geodraw.exe
