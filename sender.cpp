#include <stdio.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <signal.h>

#define PORT 12345
#define BUFSIZE 1024

int main() {
	signal(SIGPIPE, SIG_IGN);
    // ---------------------------------------------------------
    // 1. CONFIGURAÇÃO DO SERVIDOR (Executado apenas UMA vez)
    // ---------------------------------------------------------
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Erro ao criar socket do servidor");
        return 1;
    }
    
    // Ajuda o Linux a reutilizar a porta mais rapidamente
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Erro no bind. A porta já está em uso?");
        return 1;
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("Erro no listen");
        return 1;
    }

    printf("Servidor de áudio iniciado na porta %d!\n", PORT);

    // ---------------------------------------------------------
    // 2. O LOOP PRINCIPAL (Mantém o programa vivo para sempre)
    // ---------------------------------------------------------
    while(true) {
        printf("\n=> A aguardar que o tablet se conecte...\n");

        // O programa fica bloqueado aqui à espera que cliques no botão do tablet
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Falha ao aceitar cliente, tentando novamente...");
            continue; // Falhou? Tenta o próximo, não fecha o programa!
        }

        // --- OTIMIZAÇÕES DE REDE (Baixa Latência) ---
        int flag = 1;
        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
        int sndbuf = 4096;
        setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        printf("Tablet Conectado! A preparar hardware de áudio...\n");

        // --- CONFIGURAÇÃO DO PULSEAUDIO ---
        static const pa_sample_spec ss = {
            .format = PA_SAMPLE_S16LE,
            .rate = 48000,
            .channels = 2
        };

        pa_buffer_attr ba;
        ba.maxlength = 1024 * 4;
        ba.tlength = (uint32_t) -1;
        ba.prebuf = (uint32_t) -1;
        ba.minreq = (uint32_t) -1;
        ba.fragsize = 1024;

        int error;
        pa_simple *s = NULL;
        const char* device = "alsa_output.pci-0000_01_00.1.hdmi-stereo-extra5.monitor";

        s = pa_simple_new(NULL, "USB-Audio-Streamer", PA_STREAM_RECORD, device, "Record HDMI", &ss, NULL, &ba, &error);
        if (!s) {
            fprintf(stderr, "Erro ao abrir PulseAudio: %s\n", pa_strerror(error));
            close(client_sock); // Fecha o cliente
            continue;           // Volta para o início do loop (aguardar nova conexão)
        }

        pa_simple_flush(s, &error); // Limpa lixo antigo
        printf("A transmitir em tempo real!\n");

        uint8_t buffer[BUFSIZE];
        uint8_t isRunning = 1;

        while (isRunning) {
            if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0){
                fprintf(stderr, "Erro de captura: %s\n", pa_strerror(error));
                isRunning = 0;
            } 

            else if (send(client_sock, buffer, sizeof(buffer), MSG_NOSIGNAL) <= 0) {
                printf("O tablet desconectou (Botão Parar pressionado).\n");
                isRunning = 0;
            }
        }

        printf("Transmissão encerrada. A libertar hardware...\n");
        close(client_sock);     // Desliga o tablet atual
        pa_simple_free(s);      // Desliga o microfone (PulseAudio)
    }
    
    close(server_fd);
    return 0;
}
