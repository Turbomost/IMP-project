#include "MK60D10.h"
#include "board.h"
#include "clock_config.h"
#include "ethernetif.h"
#include "fsl_device_registers.h"
#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/init.h"
#include "lwip/mem.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "pin_mux.h"

// Prototypes
static const char *cgi_handler_basic(int ind, int count, char *param[], char *val[]);
// void cgi_ex_init(void);

// GPIO pin configuration
#define BOARD_LED_GPIO BOARD_LED_ORANGE_GPIO
#define BOARD_LED_GPIO_PIN BOARD_LED_ORANGE_GPIO_PIN
#define BOARD_SW_GPIO BOARD_SW1_GPIO
#define BOARD_SW_GPIO_PIN BOARD_SW1_GPIO_PIN
#define BOARD_SW_PORT BOARD_SW1_PORT
#define BOARD_SW_IRQ BOARD_SW1_IRQ
#define BOARD_SW_IRQ_HANDLER BOARD_SW1_IRQ_HANDLER

// Variable to store the button state
int button = 0;

// CGI structure
static const tCGI cgis[] = {
    {"/multi",
     cgi_handler_basic},
    {"/state",
     cgi_handler_basic}};

// Interrupt service for SysTick timer
void SysTick_Handler(void) {
    time_isr();
}

// CGI handler
static const char *cgi_handler_basic(int ind, int count, char *param[], char *val[]) {
    LWIP_ASSERT("check index", ind < LWIP_ARRAYSIZE(cgis));

    // /multi
    if (ind == 0) {
        // Set all leds off
        PTB->PDOR |= (1 << 2);
        PTB->PDOR |= (1 << 3);
        PTB->PDOR |= (1 << 4);
        PTB->PDOR |= (1 << 5);

        // Set the ones that are checked on
        for (int i = 0; i < count; i++) {
            if (!strcmp(param[i], "index") && ((val[i][0] >= '2') && (val[i][0] <= '5')))
                PTB->PDOR &= ~(1 << (val[i][0] - '0'));
        }
        return "/index.html";
    }

    // /state
    else if (ind == 1) {
        return button == 1 ? "/button_state_on.html" : "/button_state_off.html";
    }

    return "/404.html";
}

void blink_and_wait(void) {
    // Blink all leds
    for (int i = 0; i <= 3; i++) {
        PTB->PDOR |= (1 << 2);
        PTB->PDOR |= (1 << 3);
        PTB->PDOR |= (1 << 4);
        PTB->PDOR |= (1 << 5);
        for (int j = 0; j < 1000000; j++)
            ;
        PTB->PDOR &= ~(1 << 2);
        PTB->PDOR &= ~(1 << 3);
        PTB->PDOR &= ~(1 << 4);
        PTB->PDOR &= ~(1 << 5);
        for (int j = 0; j < 1000000; j++)
            ;
    }

    // Wait for button
    while ((PTE->PDIR & (1 << 12)) == 0)
        ;
    while ((PTE->PDIR & (1 << 12)) != 0)
        ;

    // Turn off all leds
    PTB->PDOR |= (1 << 2);
    PTB->PDOR |= (1 << 3);
    PTB->PDOR |= (1 << 4);
    PTB->PDOR |= (1 << 5);
}

int main(void) {
    // Turn on port clocks
    SIM->SCGC5 = SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK;
    PORTB->PCR[5] = (PORT_PCR_MUX(0x01));  // D9
    PORTB->PCR[4] = (PORT_PCR_MUX(0x01));  // D10
    PORTB->PCR[3] = (PORT_PCR_MUX(0x01));  // D11
    PORTB->PCR[2] = (PORT_PCR_MUX(0x01));  // D12
    PORTE->PCR[12] = PORT_PCR_MUX(0x01);   // SW3

    // Change port pins as outputs
    PTB->PDDR = GPIO_PDDR_PDD(0x3C);
    PTB->PDOR |= GPIO_PDOR_PDO(0x3C);

    // Set up server
    struct netif netif0;
    ip4_addr_t netif0_ip, netif0_mask, netif0_gw;

    SYSMPU_Type *base = SYSMPU;
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    base->CESR &= ~SYSMPU_CESR_VLD_MASK;

    time_init();

    IP4_ADDR(&netif0_ip, 192, 168, 0, 102);
    IP4_ADDR(&netif0_mask, 255, 255, 255, 0);
    IP4_ADDR(&netif0_gw, 192, 168, 0, 100);

    lwip_init();

    netif_add(&netif0, &netif0_ip, &netif0_mask, &netif0_gw, NULL, ethernetif_init, ethernet_input);
    netif_set_default(&netif0);
    netif_set_up(&netif0);

    httpd_init();

    // Set up CGI handlers
    http_set_cgi_handlers(cgis, LWIP_ARRAYSIZE(cgis));

    // Signal that we are ready
    blink_and_wait();

    while (1) {
        ethernetif_input(&netif0);

        // check if button 3 is pressed
        if (!(PTE->PDIR & (1 << 12))) {
            button = 1;
        } else {
            button = 0;
        }

        sys_check_timeouts();
    }

    return 0;
}
