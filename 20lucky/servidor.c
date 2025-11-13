#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define BACKLOG 5
#define BUFSZ 256

int send_line(SOCKET sock, const char *s) {
    size_t len = strlen(s);
    int n = send(sock, s, (int)len, 0);
    if (n < 0) return -1;
    if (len == 0 || s[len-1] != '\n') {
        if (send(sock, "\n", 1, 0) < 0) return -1;
    }
    return 0;
}

int recv_line(SOCKET sock, char *buf, size_t max) {
    size_t pos = 0;
    while (pos + 1 < max) {
        char c;
        int r = recv(sock, &c, 1, 0);
        if (r <= 0) {
            if (r == 0) return 0;
            return -1;
        }
        buf[pos++] = c;
        if (c == '\n') break;
    }
    buf[pos] = '\0';
    return (int)pos;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "Erro ao inicializar Winsock\n");
        return 1;
    }

    SOCKET listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == INVALID_SOCKET) { 
        perror("socket"); 
        WSACleanup();
        return 1; 
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in srv;
    memset(&srv,0,sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&srv, sizeof(srv)) < 0) { 
        perror("bind"); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }
    if (listen(listenfd, BACKLOG) < 0) { 
        perror("listen"); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }

    printf("Servidor iniciado na porta %d. Aguardando 2 jogadores...\n", port);

    struct sockaddr_in cli_addr;
    int cli_len = sizeof(cli_addr);

    SOCKET c1 = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
    if (c1 == INVALID_SOCKET) { 
        perror("accept jogador 1"); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }
    printf("Jogador 1 conectado: %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    send_line(c1, "CONECTADO 1");

    SOCKET c2 = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
    if (c2 == INVALID_SOCKET) { 
        perror("accept jogador 2"); 
        closesocket(c1); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }
    printf("Jogador 2 conectado: %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    send_line(c2, "CONECTADO 2");

    char buf1[BUFSZ], buf2[BUFSZ];

    while (1) {
        send_line(c1, "RODADA");
        send_line(c2, "RODADA");

        int r1 = recv_line(c1, buf1, BUFSZ);
        if (r1 <= 0) { printf("Jogador 1 desconectou.\n"); break; }
        int r2 = recv_line(c2, buf2, BUFSZ);
        if (r2 <= 0) { printf("Jogador 2 desconectou.\n"); break; }

        int d11, d12, d21, d22;
        char choice1 = 0, choice2 = 0;
        if (sscanf(buf1, "ROLL %d %d %c", &d11, &d12, &choice1) != 3) {
            send_line(c1, "ERRO protocolo invalido (esperado: ROLL <d1> <d2> <A|D>)");
            continue;
        }
        if (sscanf(buf2, "ROLL %d %d %c", &d21, &d22, &choice2) != 3) {
            send_line(c2, "ERRO protocolo invalido (esperado: ROLL <d1> <d2> <A|D>)");
            continue;
        }

        int val1 = (choice1 == 'A' || choice1 == 'a') ? d11 : d12;
        int val2 = (choice2 == 'A' || choice2 == 'a') ? d21 : d22;

        char info[BUFSZ];
        snprintf(info, BUFSZ, "INFO oponente rolou %d %d e escolheu %c", d21, d22, choice2);
        send_line(c1, info);
        snprintf(info, BUFSZ, "INFO oponente rolou %d %d e escolheu %c", d11, d12, choice1);
        send_line(c2, info);

        // logica de vitoria
        if ((choice1 == 'D' || choice1 == 'd') && (choice2 == 'D' || choice2 == 'd')) {
            send_line(c1, "RESULTADO EMPATE AMBOS DEFENDERAM");
            send_line(c2, "RESULTADO EMPATE AMBOS DEFENDERAM");
            printf("Ambos defenderam. RESULTADO empate automatico.\n");
            continue;
        }

        if ((choice1 == 'A' || choice1 == 'a') && (choice2 == 'A' || choice2 == 'a')) {
            if (val1 > val2) {
                send_line(c1, "RESULTADO VOCE VENCEU");
                send_line(c2, "RESULTADO VOCE PERDEU");
                printf("RESULTADO: Jogador 1 venceu (ataque %d > %d)\n", val1, val2);
                break;
            } else if (val2 > val1) {
                send_line(c2, "RESULTADO VOCE VENCEU");
                send_line(c1, "RESULTADO VOCE PERDEU");
                printf("RESULTADO: Jogador 2 venceu (ataque %d > %d)\n", val2, val1);
                break;
            } else {
                send_line(c1, "RESULTADO EMPATE ATAQUE IGUAL");
                send_line(c2, "RESULTADO EMPATE ATAQUE IGUAL");
                printf("RESULTADO: ataques iguais (%d). Empate.\n", val1);
                continue;
            }
        }

        if ((choice1 == 'A' || choice1 == 'a') && (choice2 == 'D' || choice2 == 'd')) {
            if (val1 > val2) {
                send_line(c1, "RESULTADO VOCE VENCEU");
                send_line(c2, "RESULTADO VOCE PERDEU");
                printf("RESULTADO: Jogador 1 venceu (ataque %d > defesa %d)\n", val1, val2);
                break;
            } else {
                send_line(c1, "RESULTADO EMPATE ATAQUE BLOQUEADO");
                send_line(c2, "RESULTADO EMPATE ATAQUE BLOQUEADO");
                printf("RESULTADO: Jogador 1 atacou, mas defesa %d >= ataque %d. Empate.\n", val2, val1);
                continue;
            }
        }

        if ((choice1 == 'D' || choice1 == 'd') && (choice2 == 'A' || choice2 == 'a')) {
            if (val2 > val1) {
                send_line(c2, "RESULTADO VOCE VENCEU");
                send_line(c1, "RESULTADO VOCE PERDEU");
                printf("RESULTADO: Jogador 2 venceu (ataque %d > defesa %d)\n", val2, val1);
                break;
            } else {
                send_line(c1, "RESULTADO EMPATE ATAQUE BLOQUEADO");
                send_line(c2, "RESULTADO EMPATE ATAQUE BLOQUEADO");
                printf("RESULTADO: Jogador 2 atacou, mas defesa %d >= ataque %d. Empate.\n", val1, val2);
                continue;
            }
        }

        send_line(c1, "RESULTADO CONTINUAR");
        send_line(c2, "RESULTADO CONTINUAR");
        printf("RESULTADO indefinido. Continuando...\n");
    }

    closesocket(c1);
    closesocket(c2);
    closesocket(listenfd);
    WSACleanup();
    printf("Servidor encerrado.\n");
    return 0;
}
