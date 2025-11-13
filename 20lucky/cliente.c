#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define BUFSZ 256

int send_line(SOCKET sock, const char *s) {
    size_t len = strlen(s);
    if (send(sock, s, (int)len, 0) < 0) return -1;
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

int roll_d20() {
    return (rand() % 20) + 1;
}

int main(int argc, char **argv) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Falha ao inicializar WinSock.\n");
        return 1;
    }
    
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <host> <porta> [nome]\n", argv[0]);
        WSACleanup();
        return 1;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);

    srand((unsigned)(time(NULL) ^ GetCurrentProcessId()));

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "Erro ao criar socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Resolver o hostname para endereço IP
    struct hostent *he = gethostbyname(host);
    if (he == NULL) {
        fprintf(stderr, "Erro ao resolver host '%s': %d\n", host, WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons((u_short)port);
    memcpy(&srv.sin_addr, he->h_addr_list[0], he->h_length);

    printf("Conectando ao servidor %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        fprintf(stderr, "Erro ao conectar: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Conectado com sucesso!\n");

    char buf[BUFSZ];
    // Ler mensagem inicial CONNECTED
    if (recv_line(sock, buf, BUFSZ) <= 0) {
        printf("Servidor fechou a conexão.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printf("Servidor: %s", buf);

    while (1) {
        if (recv_line(sock, buf, BUFSZ) <= 0) {
            printf("Conexão encerrada pelo servidor.\n");
            break;
        }
        
        if (strncmp(buf, "ROUND", 5) == 0) {
            // Rolar dois d20 localmente
            int d1 = roll_d20();
            int d2 = roll_d20();
            printf("\n=== NOVA RODADA ===\n");
            printf("Você rolou: d1=%d (ataque), d2=%d (defesa)\n", d1, d2);
            
            char choice = 0;
            while (1) {
                printf("Escolha (A = ataque usando d1, D = defesa usando d2): ");
                int c = getchar();
                // Descartar resto da linha
                while (getchar() != '\n' && c != EOF);
                
                if (c == 'A' || c == 'a' || c == 'D' || c == 'd') {
                    choice = (char)c;
                    break;
                }
                printf("Entrada inválida. Use A ou D.\n");
            }
            
            char out[BUFSZ];
            snprintf(out, BUFSZ, "ROLL %d %d %c", d1, d2, choice);
            send_line(sock, out);

            // Aguardar mensagens de info/result do servidor
            while (1) {
                if (recv_line(sock, buf, BUFSZ) <= 0) {
                    printf("Servidor desconectou.\n");
                    goto end;
                }
                
                if (strncmp(buf, "INFO", 4) == 0) {
                    printf("Servidor: %s", buf + 5); // Mostra info após "INFO "
                } else if (strncmp(buf, "ERROR", 5) == 0) {
                    printf("Servidor erro: %s", buf + 6);
                } else if (strncmp(buf, "RESULT", 6) == 0) {
                    printf("Servidor: %s\n", buf + 7);
                    // Se RESULT indica YOU_WIN ou YOU_LOSE -> encerra
                    if (strstr(buf, "YOU_WIN") || strstr(buf, "YOU_LOSE")) {
                        goto end;
                    } else {
                        // CONTINUE ou TIE -> continuar loop principal
                        break;
                    }
                } else {
                    printf("Servidor (outro): %s\n", buf);
                }
            }
        } else {
            printf("Servidor: %s\n", buf);
        }
    }

end:
    closesocket(sock);
    WSACleanup();
    printf("Conexão finalizada. Obrigado por jogar!\n");
    return 0;
}