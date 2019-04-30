// ======================================================================== BOF
#include "msp430x22x4.h"
#include "stdint.h"
#include "stdio.h"                           // Allows comm. with host terminal

typedef union                             // Control & Data word type for DAC
{ 
    uint8_t    u8[2];                     // 08-bits for easy write to 8-bit TX-BUF reg.
    uint16_t u16;                         // 16-bits for easy arithmetic on full word
} dacWord_t;
volatile dacWord_t dacWordOut;

#pragma vector=ADC10_VECTOR
__interrupt void IsrCaptureData (void)
// ----------------------------------------------------------------------------
// Func:    At ADC IRQ, increment Pulse Count & toggle the built-in Red LED.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{	
    dacWordOut.u16 = (0x3FF & ADC10MEM) << 2; // Save off ADC value (only the 10 bits)
	
    int8_t     i;                      // Decl. generic loop counter 
    
    // Set value of dacWord here from the ADC

    UCB0CTL1 &= ~UCSWRST;              // Enable (Un-reset) USCB0
    while (UCB0STAT & UCBUSY){};       // Wait til SPI is idle (should be now)

    // Send continuous series of values to DAC
    P2OUT &= ~0x80;                    // Assert SS (active low)

    for ( i = 1; i >= 0; i--)          // INNER LOOP: Tx 2 Bytes, MSB 1st
    { 
        UCB0TXBUF = dacWordOut.u8[i];  // Tx next byte (TXIFG auto-clr) 
        while ( !(IFG2 & UCB0TXIFG)){};// Wait til TXBUF is ready (empty) 
    }
    while (UCB0STAT & UCBUSY){};       // Wait til ALL bits shift out SPI
        
    P2OUT |= 0x80;                     // De-assert SS => End of stream
	
    P1OUT ^= 0x01;                     // Toggle the onboard LED
    P1IFG &= ~0x01;                    // Clear P1.0 IRQ Flag

   __bic_SR_register_on_exit(LPM0_bits);  // Enter AM w/ IRQs enab.
                                          // To print correctly
}

void InitPorts (void)
// ----------------------------------------------------------------------------
// Func:    Initialize Button & LED ports, and ports for I/O on TA1 Capture.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{
	
    // Config. GPIO Ports for SPI
    P3SEL = 0x0A;               // P3.1 & P3.3 = alt modes = SPI SIMO & SCLK 
    P3DIR = 0x0A;               // P3.1 & P3.3 = output dir
 
    P2SEL &= ~0x80;             // P2.7    = GPIO for SPI SS (DAC)
    P2DIR |=  0x80;             // P2.7    = output dir
    P2OUT |=  0x80;             // init. = clear SPI SS (active low)
	
    P1DIR |= 0x01;              // All pins are inputs except P1.0
    P2DIR |= 0x02;              // P2.1 = Alt mode SMCLK

    P1OUT &= ~0x01;             // Initial P1.0 = 0 => LED off

    ADC10AE0  |= 0x01;          // Enable chnl A0 = P2.0;
}

void main(void)
// ----------------------------------------------------------------------------
// Func:    Init I/O ports & IRQs, Config. Timer TA Chnl 1, enter LowPwr Mode.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{ WDTCTL = WDTPW | WDTHOLD;              // Stop Watchdog Timer
    BCSCTL1 = CALBC1_8MHZ;     // DCO = 8 MHz due to battery power
    DCOCTL  = CALDCO_8MHZ;     // DCO = 8 MHz due to battery power

    InitPorts();                         // Configure I/O Pins

    ADC10CTL0 |= ADC10ON                 // Turn ON ADC10
              |  ADC10SHT_0              // samp-hold tim = 4 cyc;
              |  ADC10IE;                // Enable flag

    ADC10CTL1 |= ADC10SSEL_3             // ADC10CLK source = SMCLK
              |  ADC10DIV_0              // ADC10CLK divider = 1
              |  INCH_0;                 // Select input = chnl A0 (default);

    // Config. USCB0 for SPI Master
    UCB0CTL0 |= UCSYNC | UCMST | UCMSB; // 3-pin SPI master, Send msb 1st
    UCB0CTL1 |= UCSSEL_2;               // SCLK Source Select SMCLK (which is
                                        // currently 16MHz from the ADC code)
    
    UCB0BR1   = 0x00;                   // SCLK Divider MSB
    UCB0BR0   = 0x01;                   // SCLK Divider LSB
    
    while (1)
    {
      ADC10CTL0 |= ADC10SC | ENC;         // start next sample
      _BIS_SR ( LPM0_bits | GIE );        // Enter LPM1 w/ IRQs enab.
    }

}
  

// ======================================================================== EOF
