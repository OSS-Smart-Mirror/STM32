#include "stm32f0xx.h"
#include "stm32f0_discovery.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int display_initialized = 0;
float temp = 0;
char LDR = 'M';
float ir = 0;
float ldr = 0;
double x;

static void nano_wait(int t);
int read_adc_channel(unsigned int channel);
void tim15_init(void);
void init_usart1(void);
void writechar(char c);
void send_to_usart(void);
void print(const char *s);
void println(const char *s);


static void nano_wait(int t) {
    asm("       mov r0,%0\n"
        "repeat:\n"
        "       sub r0,#83\n"
        "       bgt repeat\n"
        : : "r"(t) : "r0", "cc");
}

int read_adc_channel(unsigned int channel) {
	ADC1->CHSELR = 0;
	ADC1->CHSELR |= 1 << channel;
	while(!(ADC1->ISR & ADC_ISR_ADRDY));
	ADC1->CR |= ADC_CR_ADSTART;
	while(!(ADC1->ISR & ADC_ISR_EOC));
	return ADC1->DR;
}

void tim15_init(void) {
	RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
	TIM15->PSC = 4800 - 1;
	TIM15->ARR = 1000 - 1;
	TIM15->DIER |= TIM_DIER_UIE;
	NVIC->ISER[0] = 1<<TIM15_IRQn;
	TIM15->CR1 |= TIM_CR1_CEN;
}

void TIM15_IRQHandler(void) {
	TIM15->SR = ~TIM_SR_UIF;
	x = read_adc_channel(0) * 3 / 4096.0;
	temp = (x - 0.5) * 75 / 3; //PA0
	ldr = read_adc_channel(1) - 3100; //PA1
	ir = read_adc_channel(2); //PA2
}

void init_usart1(void) {
    //enable port A
    RCC->AHBENR = RCC_AHBENR_GPIOAEN;
    //PA9 USART1_TX
    //PA10 USART1_RX
    // Pins 2 and 3 (make it alternate functions)
    GPIOA->MODER &= ~(3<<18);
    GPIOA->MODER |= 2<<18;

    GPIOA->MODER &= ~(3<<20);
    GPIOA->MODER |= 2<<20;

    //Alternate Function low register
    GPIOA->AFR[1] &= ~0xffff;
    GPIOA -> AFR[1] |= 0x110;

    //Set the output speed register to high speed for TX (PA9)
    GPIOA -> OSPEEDR &= ~(3<<18);
    GPIOA -> OSPEEDR |= 3<<18;

    // Configure USART1
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // Disable USART
    USART1->CR1 &= ~USART_CR1_UE;

    // Configure USART1 for 115200 baud operation with 8 bits
    USART1->BRR = 0x1A1;

    //16 bit oversampling
    USART1->CR1 &= ~USART_CR1_OVER8;

    //Disable Parity control
    USART1->CR1 &= ~USART_CR1_PCE;

    //One bit stop
    USART1->CR2 &= ~USART_CR2_STOP;

    //Set it to be 1 start bit, 8 data bit
    //USART1->CR1 |= 0x10000000;

    // Enable transmitter
    USART1->CR1 |= USART_CR1_TE;
    // Enable receiver
    USART1->CR1 |= USART_CR1_RE;
    // Enable USART
    USART1->CR1 |= USART_CR1_UE;
}

void writechar(char c) {
    //Waits for "transitter empty" flag to be true
    while((USART1->ISR & USART_ISR_TXE) != USART_ISR_TXE);

    if(c == '\n')
    {
        USART1->TDR = '\r';
    }

    while((USART1->ISR & USART_ISR_TXE) != USART_ISR_TXE);
    USART1->TDR = c;

}

void send_to_usart(void) {
    char buffer[50];
    for(;;) {
    	nano_wait(50000000);
    	ldr = (5.0 * read_adc_channel(0)) / 4096.0; //PA0
    	if (ldr < 1.5) {
    		LDR = 'L';
    	} else if (ldr > 4) {
    		LDR = 'H';
    	} else {
    		LDR = 'M';
    	}
    	nano_wait(50000000);
    	ir = (5.0 * read_adc_channel(1)) / 4096.0; //PA1
    	if (ir > 4.7) {
    		nano_wait(50000000);
    		temp = 23.0 * ((5.0 * read_adc_channel(2) / 4096.0) - 0.9); //PA2
    	}
    	sprintf(buffer, "%.1f,%c,%.1f", temp, LDR, ir);
    	println(buffer);
    }
}

//============================================================================
// print()
// Write a string to the serial port output.
// Return the length of the string.
//
void print(const char *s) {
    const char *ptr;
    for(ptr=s; *ptr; ptr+=1)
        writechar(*ptr);
}

void println(const char *s) {
    print(s);
    print("\n");
}

void setup_gpio() {
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 3<<0;
    GPIOA->MODER |= 3<<2;
    GPIOA->MODER |= 3<<4;
}

void setup_adc() {
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
	RCC->CR2 |= RCC_CR2_HSI14ON;
	ADC1->CR |= ADC_CR_ADEN;
	while(!(ADC1->ISR & ADC_ISR_ADRDY));
}

int main(void)
{
	setup_gpio();
	setup_adc();
    init_usart1();
    send_to_usart();
    return 0;
}
