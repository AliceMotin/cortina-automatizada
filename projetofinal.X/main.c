#include <xc.h>
#include <pic16f877a.h>  
#include <stdio.h> 
#include <xc.h>
#include <stdint.h>

#define _XTAL_FREQ 4000000

// Configura??o do PIC16F877A
#pragma config FOSC = HS        // Oscilador em modo HS
#pragma config WDTE = OFF       // Desabilita o Watchdog Timer
#pragma config PWRTE = OFF      // Desabilita o Power-up Timer
#pragma config BOREN = OFF      // Desabilita o Brown-out Reset
#pragma config LVP = OFF        // Desabilita a programa??o de baixa voltagem
#pragma config CPD = OFF        // Desabilita a prote??o de c?digo de dados
#pragma config WRT = OFF        // Desabilita a grava??o no c?digo de programa
#pragma config CP = OFF         // Desabilita a prote??o de c?digo de programa

// Defini??es de pinos
#define LDR_PIN     RA0       // Pino anal?gico conectado ao LDR
#define MANUAL RB3

//variaveis
int timerCounter = 0;
int ledState = 0;

int fim = 0;
int inicio = 1;
int meio = 0;
int aberta = 1;
int opening = 0;
int closing = 0;

int manualControl = 0;  // 0 for automatic control, 1 for manual control

// Definition For PWM1_Set_Duty Function
void PWM1_Set_Duty(uint16_t);

// Fun??o de inicializa??o
void initialize() {
    // Configura??o do PORTA como entrada para o LDR
    TRISB = 0b11111111;       //configura pinos de entrada 1
    TRISD = 0b00000000;       //configura pinos de saida 0
    TRISA = 0b11111111;       //configura pinos de entrada 1
    TRISC = 0b00000000;       //configura pinos de saida 0
    PORTD = 0;
    PORTC = 1;
    OPTION_REGbits.nRBPU = 0; //Ativa resistores de pull-ups
}

// Fun??o para leitura do LDR
int readLDR() {
    ADCON0 = 0b00000001;     // Seleciona canal RA0
    ADCON1 = 0b00001110;     // Configura o resultado da convers?o como direita justificada
    
    // Inicia a convers?o ADC
    GO_nDONE = 1;
    
    // Aguarda o t?rmino da convers?o
    while (GO_nDONE);
    
    // Retorna o valor convertido
    return (ADRESH << 8) | ADRESL;
}

//pisca leds indicadores quando a cortina est  abrindo ou fechando
void __interrupt() ISR(void) {
    // Timer0 overflow interrupt
    if (TMR1IF) {
        timerCounter++;
        if (timerCounter == 1 ) {  // Adjust this value for desired blink speed
            ledState = !ledState;
            if (closing == 1) {
                RD5 = ledState;
            }
            if (opening == 1) {
                RD6 = ledState;
            }
            timerCounter = 0;  // Reset counter
            PIR1bits.TMR1IF = 0; //reseta o flag da interrup??o
            TMR1L = 0xff;        //reinicia contagem com 65430
            TMR1H = 0x9c; 
        }
        T0IF = 0;  // Clear Timer0 interrupt flag
    }
}

// Fun??o principal
void main() {
    initialize();
    
    // Set up Timer0 for interrupts (adjust the pre-scaler for desired blink speed)
    //** configurando interrup??es ***********************************
    INTCONbits.GIE=1;       //Habiliita a int global
    INTCONbits.PEIE = 1;    //Habilita a int dos perif?ricos
    PIE1bits.TMR1IE = 1;    //Habilita int do timer 1

    //*** configura o timer 1 *****************************************
    T1CONbits.TMR1CS = 0;   //Define timer 1 como temporizador (Fosc/4)
    T1CONbits.T1CKPS0 = 1;  //bit pra configurar pre-escaler, nesta caso 1:8
    T1CONbits.T1CKPS1 = 1;  //bit pra configurar pre-escaler, nesta caso 1:8

    TMR1L = 0xff;          //carga do valor inicial no contador (65536-65436) =
    TMR1H = 0x9c;          //100   

    T1CONbits.TMR1ON = 1;   //Liga o timer
    
    // pinos de controle do motor inicialmente parado 
    RC0 = 0;
    RC1 = 0;

    //--[ Configure The CCP Module For PWM Mode ]--
    CCP1M3 = 1;
    CCP1M2 = 1;
    TRISC2 = 0; // The CCP1 Output Pin (PWM)
    // Set The PWM Frequency (2kHz)
    PR2 = 124;
    // Set The PS For Timer2 (1:4 Ratio)
    T2CKPS0 = 1;
    T2CKPS1 = 0;
    // Start CCP1 PWM !
    TMR2ON = 1;
    
    RD7 = 0;
    // The Main Loop (Routine)


while (1)
{
    int ldrValue = readLDR();
    
    //button manual control  
    if(MANUAL == 0){  
        __delay_ms(50);
        manualControl = ~manualControl ; 
        RD7 =~ RD7;
    }
    //botoes fim de curso
    if (RB5 == 0 && RC0 == 1 && RC1 == 0){  //FIM 
        fim = 1;
        meio = 0;
        PWM1_Set_Duty(0);
        RC0 = 0;
        RC1 = 0;
        aberta = 0;
        opening = 0;
        closing = 0;
    }
    if (RB6 == 0) { //MEIO
        meio = 1;
        PWM1_Set_Duty(50);
    }
    if (RB7 == 0 && RC0 == 0 && RC1 == 1) { //INICIO 
        inicio = 1;
        meio = 0;
        PWM1_Set_Duty(0);
        RC0 = 0;
        RC1 = 0;
        aberta = 1;
        closing = 0;
        opening = 0;
    }
    
    //leds
    //aberta
    if (inicio == 1) {
        RD5 = 0; 
        RD6 = 1;   
    } 
    //fechada
    if (fim == 1) {
        RD5 = 1;
        RD6 = 0;
    }
    //fechando
    if (inicio == 0 && closing == 1) {
        RD6 = 0;
    }
    //abrindo
    if (fim == 0 && opening == 1) {
        RD5 = 0;
    }

    if (manualControl == 0)
        {
            // Automatic control based on light sensor
            // open the curtain
            if (ldrValue > 1500 && fim != 0 && opening == 0)
            {
                RC0 = 0;
                RC1 = 1;
                
                if (meio == 1) {
                    PWM1_Set_Duty(100);
                } else {
                    PWM1_Set_Duty(200);
                }
                
                fim = 0;
                inicio = 0;
                opening = 1;
                closing = 0;
            }
            // close the curtain  
            else if(ldrValue < 1000 && inicio != 0 && closing == 0)
            {
                RC0 = 1;
                RC1 = 0;
                
                if (meio == 1) {
                    PWM1_Set_Duty(100);
                } else {
                    PWM1_Set_Duty(200);
                }
                
                inicio = 0;
                fim = 0;
                closing = 1;
                opening = 0;
            }
        }
        else 
        {
            // Manual control based on buttons //fechar
            if (RB0 == 0 && inicio == 1)  
            {
                RC0 = 1;
                RC1 = 0;
                
                if (meio == 1) {
                    PWM1_Set_Duty(100);
                } else {
                    PWM1_Set_Duty(200);
                }
                
                inicio = 0;
                fim = 0;
                closing = 1;
                opening = 0;
            }
            else if(RB1 == 0 && fim == 1)  //abrir
            {
                RC0 = 0;
                RC1 = 1;
                
                if (meio == 1) {
                    PWM1_Set_Duty(100);
                } else {
                    PWM1_Set_Duty(200);
                }
                
                fim = 0;
                inicio = 0;
                opening = 1;
                closing = 0;
            }
        }

        // Add a delay for stability
        __delay_ms(20);
    }
}

void PWM1_Set_Duty(uint16_t DC)
{
  // Check The DC Value To Make Sure it's Within 10-Bit Range
  if(DC < 1024)
  {
    CCP1Y = DC & 1;
    CCP1X = DC & 2;
    CCPR1L = DC >> 2;
  }
}