gcc -o servidor.exe servidor.c -lws2_32

gcc -o cliente.exe cliente.c -lws2_32

servidor.exe 9090

cliente.exe 127.0.0.1 9090 jogador1

cliente.exe 127.0.0.1 9090 jogador2