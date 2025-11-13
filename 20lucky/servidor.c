#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define BACKLOG 5
#define BUFSZ 256

// envia uma string terminada por '\n'
int send_line(SOCKET sock, const char *s) {
    size_t len = strlen(s);
    int n = send(sock, s, (int)len, 0);
    if (n < 0) return -1;
    // envia newline se não houver
    if (len == 0 || s[len-1] != '\n') {
        if (send(sock, "\n", 1, 0) < 0) return -1;
    }
    return 0;
}

// recebe uma linha até '\n' (ou EOF). retorna número de bytes lidos, 0=fechou
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

    // Inicializar Winsock PRIMEIRO
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
        perror("accept c1"); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }
    printf("Jogador 1 conectado: %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    send_line(c1, "CONNECTED 1");

    SOCKET c2 = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
    if (c2 == INVALID_SOCKET) { 
        perror("accept c2"); 
        closesocket(c1); 
        closesocket(listenfd); 
        WSACleanup();
        return 1; 
    }
    printf("Jogador 2 conectado: %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    send_line(c2, "CONNECTED 2");

    char buf1[BUFSZ], buf2[BUFSZ];

    while (1) {
        // iniciar rodada
        send_line(c1, "ROUND");
        send_line(c2, "ROUND");

        // receber respostas dos dois
        int r1 = recv_line(c1, buf1, BUFSZ);
        if (r1 <= 0) { printf("Jogador 1 desconectou.\n"); break; }
        int r2 = recv_line(c2, buf2, BUFSZ);
        if (r2 <= 0) { printf("Jogador 2 desconectou.\n"); break; }

        // parse
        int d11, d12, d21, d22;
        char choice1 = 0, choice2 = 0;
        if (sscanf(buf1, "ROLL %d %d %c", &d11, &d12, &choice1) != 3) {
            send_line(c1, "ERROR protocolo invalido (esperado: ROLL <d1> <d2> <A|D>)");
            continue;
        }
        if (sscanf(buf2, "ROLL %d %d %c", &d21, &d22, &choice2) != 3) {
            send_line(c2, "ERROR protocolo invalido (esperado: ROLL <d1> <d2> <A|D>)");
            continue;
        }

        // determina valores escolhidos
        int val1 = (choice1 == 'A' || choice1 == 'a') ? d11 : d12;
        int val2 = (choice2 == 'A' || choice2 == 'a') ? d21 : d22;

        // informar aos dois o que o outro fez
        char info[BUFSZ];
        snprintf(info, BUFSZ, "INFO opponent rolled %d %d and chose %c", d21, d22, choice2);
        send_line(c1, info);
        snprintf(info, BUFSZ, "INFO opponent rolled %d %d and chose %c", d11, d12, choice1);
        send_line(c2, info);

        // lógica de vitória
        if ((choice1 == 'A' || choice1 == 'a') && (choice2 == 'A' || choice2 == 'a')) {
            if (val1 < val2) {
                send_line(c1, "RESULT YOU_WIN");
                send_line(c2, "RESULT YOU_LOSE");
                printf("Resultado: jogador 1 venceu (ataques %d < %d)\n", val1, val2);
                break;
            } else if (val2 < val1) {
                send_line(c2, "RESULT YOU_WIN");
                send_line(c1, "RESULT YOU_LOSE");
                printf("Resultado: jogador 2 venceu (ataques %d < %d)\n", val2, val1);
                break;
            } else {
                // igualdade de ataque -> continuar
                send_line(c1, "RESULT TIE_ATTACK_CONTINUE");
                send_line(c2, "RESULT TIE_ATTACK_CONTINUE");
                printf("Ataques iguais (%d). Continuando...\n", val1);
                continue;
            }
        } else {
            // qualquer outra combinação -> continuar
            send_line(c1, "RESULT CONTINUE");
            send_line(c2, "RESULT CONTINUE");
            printf("Nenhum vencedor nessa rodada (choices %c vs %c). Continuando...\n", choice1, choice2);
            continue;
        }
    }

    closesocket(c1);
    closesocket(c2);
    closesocket(listenfd);
    WSACleanup();
    printf("Servidor encerrado.\n");
    return 0;
}