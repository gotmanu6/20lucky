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
        if (r <= 0) return r;
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
    printf("|               20lucky                |\n");
    printf("========================================\n");
    printf("|  [1] Jogar                           |\n");
    printf("|  [2] Creditos                        |\n");
    printf("|  [3] Sair                            |\n");
    printf("========================================\n");
}

int main(int argc, char **argv) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (argc < 3) {
        printf("Uso: %s <host> <porta>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);

    srand((unsigned)(time(NULL)));

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    struct hostent *he = gethostbyname(host);
    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    memcpy(&srv.sin_addr, he->h_addr_list[0], he->h_length);

    printf("Conectando ao servidor...\n");
    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        printf("Erro ao conectar.\n");
        return 1;
    }

    char buf[BUFSZ];
    recv_line(sock, buf, BUFSZ);
    printf("Servidor: %s", buf);

    int opcao = 0;
    while (1) {
        mostrar_menu();
        printf("Escolha: ");
        scanf("%d", &opcao);
        while (getchar() != '\n');

        if (opcao == 1) break;
        if (opcao == 2) {
            printf("=== CREDITOS ===\n");
            printf("Emanuelly Prestes Lopes RA: 2417422922\n");
            printf("Clara Ishida RA: 24027647-2\n");
            printf("Guilherme Alves da Silva RA: 24021322-2\n\n");
        }
        if (opcao == 3) {
            printf("Saindo...\n");
            closesocket(sock);
            WSACleanup();
            return 0;
        }
    }

    while (1) {
        if (recv_line(sock, buf, BUFSZ) <= 0) break;

        if (strncmp(buf, "RODADA", 6) == 0) {

            int d1 = roll_d20();
            int d2 = roll_d20();
            printf("\n=== NOVA RODADA ===\n");
            printf("Voce rolou ataque=%d defesa=%d\n", d1, d2);

            char escolha;
            while (1) {
                printf("Escolha (A=ataque / D=defesa): ");
                escolha = getchar();
                while (getchar() != '\n');
                if (escolha=='A'||escolha=='a'||escolha=='D'||escolha=='d') break;
                printf("Opcao invalida.\n");
            }

            char out[BUFSZ];
            snprintf(out, BUFSZ, "ROLL %d %d %c", d1, d2, escolha);
            send_line(sock, out);

            while (1) {
                recv_line(sock, buf, BUFSZ);

                if (strncmp(buf, "INFO", 4) == 0) {
                    printf("Info: %s\n", buf + 5);
                }
                else if (strncmp(buf, "ERRO", 4) == 0) {
                    printf("Erro: %s\n", buf + 5);
                }
                else if (strncmp(buf, "RESULTADO", 9) == 0) {
                    printf("%s\n", buf);
                    if (strstr(buf, "VENCEU") || strstr(buf, "PERDEU"))
                        goto fim;
                    break;
                }
                else {
                    printf("Servidor disse: %s\n", buf);
                }
            }
        }
        else {
            printf("Servidor: %s\n", buf);
        }
    }

fim:
    closesocket(sock);
    WSACleanup();
    printf("Jogo encerrado.\n");
    return 0;
}
