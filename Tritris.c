#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "ws2812.pio.h" 

#define MAT_LED_PIN 7
#define BOTAO_A_PIN 5
#define BOTAO_B_PIN 6

#define NUM_LEDS 25
#define NUM_LINHAS 5
#define NUM_COLUNAS 5
#define LARG_PECAS 2
#define COMP_PECAS 2

int matriz[NUM_LINHAS][NUM_COLUNAS] = {};

int pecaHorizontal[LARG_PECAS][COMP_PECAS] = {{1,1},{0,0}};
int pecaVertical[LARG_PECAS][COMP_PECAS] = {{1,0},{1,0}};

int pecaAtiva[LARG_PECAS][COMP_PECAS] = {}; 

int x = 0;
int y = 0;

PIO pio;
uint sm;

int mapearIndiceLED(int linha, int coluna) {
    int linhaInvertida = NUM_LINHAS - 1 - linha;

    if (linhaInvertida % 2 == 0) {
        return linhaInvertida * NUM_COLUNAS + (NUM_COLUNAS - 1 - coluna);
    } else {
        return linhaInvertida * NUM_COLUNAS + coluna;
    }
}

void limparLeds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, 0x000000);
    }
    sleep_ms(1);
}

void limparMatriz(){
    memset(matriz, 0, sizeof(matriz));
}

// void imprimirMatriz() {
//     printf("\nEstado da Matriz:\n");
//     for (int i = 0; i < NUM_LINHAS; i++) {
//         for (int j = 0; j < NUM_COLUNAS; j++) {
//             printf("%d ", matriz[i][j]);
//         }
//         printf("\n");
//     }
// }

void atualizarLeds() {
    uint32_t leds[NUM_LEDS] = {0};

    for (int i = 0; i < NUM_LINHAS; i++) {
        for (int j = 0; j < NUM_COLUNAS; j++) {
            int ledIndex = mapearIndiceLED(i, j);
            if (matriz[i][j] == 1) {
                leds[ledIndex] = 0xFF0000; // Vermelho para peças fixadas
            } else if (matriz[i][j] == 2) {
                leds[ledIndex] = 0x00FF00; // Verde para a peça em movimento
            } else {
                leds[ledIndex] = 0x000000; // Apagado
            }
        }
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, leds[i]);
    }
}


bool colisao(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            if (peca[i][j] == 1) {
                if (y + i >= NUM_LINHAS || x + j < 0 || x + j >= NUM_COLUNAS || matriz[y + i][x + j] == 1) {
                    return true;
                }
            }
        }
    }
    return false; 
}

void limparLinha() {
    // Verifica se alguma linha está preenchida
    for (int i = 0; i < NUM_LINHAS; i++) {
        bool linha_completa = true;
        for (int j = 0; j < NUM_COLUNAS; j++) {
            if (matriz[i][j] == 0 || matriz[i][j] == 2) {
                linha_completa = false;
                break;
            }
        }
        // Se sim, limpa a linha e desce as linhas acima
        if (linha_completa) {
            for (int j = 0; j < NUM_COLUNAS; j++) {
                matriz[i][j] = 0;
            }
            for (int k = i; k > 0; k--) {
                for (int j = 0; j < NUM_COLUNAS; j++) {
                    matriz[k][j] = matriz[k - 1][j];
                }
            }
        }
    }
}

void fixar(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            if (peca[i][j] == 1) {
                matriz[y + i][x + j] = 1;
            }
        }
    }
    limparLinha();
}

void criarNovaPeca() {
    int escolha = rand() % 2; 
    if (escolha == 0) {
        memcpy(pecaAtiva, pecaHorizontal, sizeof(pecaHorizontal));
    } else {
       memcpy(pecaAtiva, pecaVertical, sizeof(pecaVertical));
    }
    atualizarLeds(pio, sm);


    x = 0;
    y = 0;


    if (colisao(x, y, pecaAtiva)) {
        int tempo = 0;
        int tempoMaximo = 2000;
        limparMatriz();
        while (tempo <= tempoMaximo){
            for(int i = 0; i < NUM_LEDS; i++){
                pio_sm_put_blocking(pio, sm, 0x0FF000);
            }
            tempo += 1000;
            sleep_ms(1000);
        }
        atualizarLeds();
    }
}

void atualizarMatrizComPeca(int x, int y, int peca[LARG_PECAS][COMP_PECAS]) {
    for (int i = 0; i < NUM_LINHAS; i++) {
        for (int j = 0; j < NUM_COLUNAS; j++) {
            if (matriz[i][j] == 2) {
                matriz[i][j] = 0;
            }
        }
    }

    for (int i = 0; i < LARG_PECAS; i++) {
        for (int j = 0; j < COMP_PECAS; j++) {
            if (peca[i][j] == 1 && y + i < NUM_LINHAS && x + j < NUM_COLUNAS) {
                matriz[y + i][x + j] = 2;
            }
        }
    }
}

void gravidade(int* x, int* y, int peca[LARG_PECAS][COMP_PECAS]) {
    if (!colisao(*x, *y + 1, peca)) {
        (*y)++;
        atualizarMatrizComPeca(*x, *y, pecaAtiva);
    } else {
        fixar(*x, *y, peca);
        criarNovaPeca(pio,sm);
    }
    atualizarLeds();
}

void moverPeca(int dx, int dy) {
    if (!colisao(x + dx, y + dy, pecaAtiva)) {
        x += dx;
        y += dy;
    }
    atualizarMatrizComPeca(x, y, pecaAtiva);
    atualizarLeds();
}

void botaoA_interrupt_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A_PIN) {
        moverPeca(-1, 0);  // Move a peça para a esquerda
        atualizarLeds();  // Atualiza os LEDs
    }
}

void botaoB_interrupt_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_B_PIN) {
        moverPeca(1, 0);   // Move a peça para a direita
        atualizarLeds();  // Atualiza os LEDs
    }
}

void configurarBotoesDeInterrupcao() {
    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL, true, &botaoA_interrupt_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true, &botaoB_interrupt_handler);
}

int main() {

    stdio_init_all();
    adc_init();
    gpio_init(BOTAO_A_PIN);
    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN); 
    gpio_pull_up(BOTAO_B_PIN);

    pio = pio0;
    
    sm = pio_claim_unused_sm(pio, true);


    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, MAT_LED_PIN, 800000, false);

    limparLeds();
    sleep_ms(5000);

    criarNovaPeca();

    configurarBotoesDeInterrupcao();

    int contador_gravidade = 0;
    int intervalo_gravidade = 1000;

    bool atualizar_leds = true;

    while (1) {

        if(contador_gravidade >= intervalo_gravidade){
            gravidade(&x, &y, pecaAtiva);
            contador_gravidade = 0;
            bool atualizar_leds = true;
        }
        
        contador_gravidade += 100;
        sleep_ms(100);
        
    }
}

