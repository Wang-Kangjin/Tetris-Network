all : TetrisServer TetrisClinet
TetrisServer: TetrisServer.c
	gcc -o TetrisServer TetrisServer.c -lpthread -lncurses
TetrisClinet: TetrisClinet.c
	gcc -o TetrisClinet TetrisClinet.c -lpthread -lncurses
