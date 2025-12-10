b:
	gcc -o build/$(exe)  src/$(exe).c `sdl2-config --cflags --libs` -lm -g
r:
	build/$(exe) < img/$(file)

br:
	gcc -o build/$(exe)  src/$(exe).c `sdl2-config --cflags --libs` -lm -g && build/$(exe) < img/$(file)