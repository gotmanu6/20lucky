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

void mostrar_menu() {
    printf("      _______           _______\n");
    printf("    /\\       \\       /\\       \\\n");
    printf("   /  \\   o   \\     /  \\ o     \\\n");
    printf("  / o  \\_______\\   /    \\_______\\\n");
    printf("  \\    / o     /   \\  o  /       /\n");
    printf("   \\  /   o   /     \\    / o    /\n");
    printf("    \\/_______/       \\/_______/\n\n");

    printf("========================================\n");
    printf("|         JOGO ATAQUE E DEFESA         |\n");
    printf("========================================\n");
    printf("|  [1] Jogar                           |\n");
    printf("|  [2] Creditos                        |\n");
    printf("|  [3] Sair                            |\n");
    printf("========================================\n");
    printf("Escolha uma opcao: ");
}



int main(int argc, char **argv) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Falha ao inicializar WinSock.\n");
        return 1;
    }
    
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <host> <porta>\n", argv[0]);
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
    if (recv_line(sock, buf, BUFSZ) <= 0) {
        printf("Servidor fechou a conexao.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printf("Servidor: %s", buf);

    int opcao = 0;
    while (1) {
        mostrar_menu();
        printf("Escolha uma opcao: ");
        if (scanf("%d", &opcao) != 1) {
            while (getchar() != '\n');
            printf("Opcao invalida!\n");
            continue;
        }
        while (getchar() != '\n');

        if (opcao == 1) {
            printf("\nIniciando o jogo...\n\n");
            break;
        } else if (opcao == 2) {
            printf("\n=== CREDITOS ===\n");
            printf("Emanuelly Prestes Lopes\n");
            printf("Clara Ishida\n\n");
        } else if (opcao == 3) {
            printf("\nSaindo do jogo... Ate mais!\n");
            closesocket(sock);
            WSACleanup();
            return 0;
        } else {
            printf("Opcao invalida! Tente novamente.\n");
        }
    }

    // --- JOGO COMECA AQUI ---
    while (1) {
        if (recv_line(sock, buf, BUFSZ) <= 0) {
            printf("Conexao encerrada pelo servidor.\n");
            break;
        }
        
        if (strncmp(buf, "ROUND", 5) == 0) {
            int d1 = roll_d20();
            int d2 = roll_d20();
            printf("\n=== NOVA RODADA ===\n");
            printf("Voce rolou: d1=%d (ataque), d2=%d (defesa)\n", d1, d2);
            
            char choice = 0;
            while (1) {
                printf("Escolha (A = ataque, D = defesa): ");
                int c = getchar();
                while (getchar() != '\n' && c != EOF);
                
                if (c == 'A' || c == 'a' || c == 'D' || c == 'd') {
                    choice = (char)c;
                    break;
                }
                printf("Entrada invalida. Use A ou D.\n");
            }
            
            char out[BUFSZ];
            snprintf(out, BUFSZ, "ROLL %d %d %c", d1, d2, choice);
            send_line(sock, out);

            while (1) {
                if (recv_line(sock, buf, BUFSZ) <= 0) {
                    printf("Servidor desconectou.\n");
                    goto end;
                }
                
                if (strncmp(buf, "INFO", 4) == 0) {
                    printf("Servidor: %s", buf + 5);
                } else if (strncmp(buf, "ERROR", 5) == 0) {
                    printf("Servidor erro: %s", buf + 6);
                } else if (strncmp(buf, "RESULT", 6) == 0) {
                    printf("Servidor: %s\n", buf + 7);
                    if (strstr(buf, "YOU_WIN") || strstr(buf, "YOU_LOSE")) {
                        goto end;
                    } else {
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
    printf("Conexao finalizada. Obrigado por jogar!\n");
    return 0;
}
