O jogo é preciso ser compilado e rodado em um sistema operacional Windows pelo command prompt

Para rodar o servidor é preciso passar uma porta

Para rodar o cliente é preciso passar a porta do servidor e o nome do jogador

gcc -o servidor.exe servidor.c -lws2_32

gcc -o cliente.exe cliente.c -lws2_32

servidor.exe 9090

cliente.exe 127.0.0.1 9090 jogador1

cliente.exe 127.0.0.1 9090 jogador2