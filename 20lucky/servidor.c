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
    send(sock, s, (int)len, 0);
    if (len == 0 || s[len-1] != '\n')
        send(sock, "\n", 1, 0);
    return 0;
}

int recv_line(SOCKET sock, char *buf, size_t max) {
    size_t pos = 0;
    while (pos + 1 < max) {
        char c;
        int r = recv(sock, &c, 1, 0);
        if (r <= 0) return r;
        buf[pos++] = c;
        if (c == '\n') break;
    }
    buf[pos] = '\0';
    return (int)pos;
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("Uso: %s <porta>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET listenfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    bind(listenfd, (struct sockaddr*)&srv, sizeof(srv));
    listen(listenfd, BACKLOG);

    printf("Servidor iniciado na porta %d.\n", port);

    struct sockaddr_in cli;
    int clilen = sizeof(cli);

    SOCKET c1 = accept(listenfd, (struct sockaddr*)&cli, &clilen);
    send_line(c1, "CONECTADO JOGADOR 1");

    SOCKET c2 = accept(listenfd, (struct sockaddr*)&cli, &clilen);
    send_line(c2, "CONECTADO JOGADOR 2");

    char buf1[BUFSZ], buf2[BUFSZ];

    while (1) {
        send_line(c1, "RODADA");
        send_line(c2, "RODADA");

        recv_line(c1, buf1, BUFSZ);
        recv_line(c2, buf2, BUFSZ);

        int d11, d12, d21, d22;
        char e1, e2;

        if (sscanf(buf1, "ROLL %d %d %c", &d11, &d12, &e1) != 3) {
            send_line(c1, "ERRO Protocolo invalido");
            continue;
        }
        if (sscanf(buf2, "ROLL %d %d %c", &d21, &d22, &e2) != 3) {
            send_line(c2, "ERRO Protocolo invalido");
            continue;
        }

        int v1 = (e1 == 'A' || e1 == 'a') ? d11 : d12;
        int v2 = (e2 == 'A' || e2 == 'a') ? d21 : d22;

        char msg[BUFSZ];
        snprintf(msg, BUFSZ, "INFO Oponente rolou %d %d e escolheu %c", d21, d22, e2);
        send_line(c1, msg);

        snprintf(msg, BUFSZ, "INFO Oponente rolou %d %d e escolheu %c", d11, d12, e1);
        send_line(c2, msg);

        if ((e1 == 'D' || e1 == 'd') && (e2 == 'D' || e2 == 'd')) {
            send_line(c1, "RESULTADO Ambos defenderam. Empate.");
            send_line(c2, "RESULTADO Ambos defenderam. Empate.");
            continue;
        }

        if ((e1 == 'A' || e1 == 'a') && (e2 == 'A' || e2 == 'a')) {
            if (v1 > v2) {
                send_line(c1, "RESULTADO VOCE VENCEU");
                send_line(c2, "RESULTADO VOCE PERDEU");
                break;
            } else if (v2 > v1) {
                send_line(c2, "RESULTADO VOCE VENCEU");
                send_line(c1, "RESULTADO VOCE PERDEU");
                break;
            } else {
                send_line(c1, "RESULTADO Empate: valores iguais");
                send_line(c2, "RESULTADO Empate: valores iguais");
                continue;
            }
        }

        if ((e1 == 'A'||e1=='a') && (e2=='D'||e2=='d')) {
            if (v1 > v2) {
                send_line(c1, "RESULTADO VOCE VENCEU");
                send_line(c2, "RESULTADO VOCE PERDEU");
                break;
            } else {
                send_line(c1, "RESULTADO Ataque bloqueado. Empate.");
                send_line(c2, "RESULTADO Ataque bloqueado. Empate.");
                continue;
            }
        }

        if ((e1=='D'||e1=='d') && (e2=='A'||e2=='a')) {
            if (v2 > v1) {
                send_line(c2, "RESULTADO VOCE VENCEU");
                send_line(c1, "RESULTADO VOCE PERDEU");
                break;
            } else {
                send_line(c1, "RESULTADO Ataque bloqueado. Empate.");
                send_line(c2, "RESULTADO Ataque bloqueado. Empate.");
                continue;
            }
        }
    }

    closesocket(c1);
    closesocket(c2);
    closesocket(listenfd);
    WSACleanup();
    return 0;
}
