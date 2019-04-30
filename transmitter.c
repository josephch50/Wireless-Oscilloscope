// ======================================================================== BOF
#include "msp430x22x4.h"         // Large MSP header
#include "stdint.h"              // Standard Integers for MSP
#include "wireless-v2.h"         // Include the wireless header file
#include "stdio.h"

typedef union                    // Control & Data word type for DAC
{
  uint16_t u16;                  // 16-bits for easy arithmetic on full word
  uint8_t  u8[2];                // 08-bits for easy write to 8-bit TX-BUF reg.
} dacWord_t;

volatile dacWord_t dacWordOut;   // Global instance of union

#pragma vector=ADC10_VECTOR
__interrupt void IsrCaptureData (void)
// ----------------------------------------------------------------------------
// Func:    At ADC IRQ, capture the value from ADC10MEM, shift it over for the
//          DAC, and transfer it over to the cc2500 to send over wireless. Once
//          completed, the ISR will exit and be in Active Mode to then kick-off
//          the next ADC capture.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{
  static uint8_t pktLen = 3;          // Fix Packet size
  
  // Save off ADC value (only the 10 bits)
  dacWordOut.u16 = ( 0x3FF & ADC10MEM );

  uint8_t pktData[3] = {0x02, dacWordOut.u8[0], dacWordOut.u8[1]}; 
   // 10b Number in 2 bytes
   // Contents:   pktData[3] = {Pyld leng, LSB, MSB}

  RFSendPacket( pktData, pktLen );    // Activate TX mode & transmit the pkt

  TI_CC_SPIStrobe(TI_CCxxx0_SIDLE);   // Set cc2500 to IDLE mode.
                                      // TX mode re-activates in RFSendPacket
                                      // w/ AutoCal @ IDLE to TX Transition

  __bic_SR_register_on_exit( LPM0_bits ); // Enter AM w/ IRQs enab.
}

void SetupAll (void)
// ----------------------------------------------------------------------------
// Func:    Initialize Button & LED ports, and ports for I/O on TA1 Capture.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{
  dacWordOut.u16 = 0x0000;
  volatile uint16_t delay;             // Counter for delay timeout

  for( delay = 0; delay < 650; delay++ ) { }; // Empirical: cc2500 Power up

  // Set up clock system
  BCSCTL1 = CALBC1_8MHZ;               // DCO = 8 MHz due to battery power
  DCOCTL  = CALDCO_8MHZ;               // DCO = 8 MHz due to battery power
  // Wireless Initialization
  P2SEL = 0;                           // P2.6, P2.7 = GDO0, GDO2 (GPIO)
  TI_CC_SPISetup();                    // Initialize SPI port for both the
                                       // SPI communication b/w cc2500 and
                                       // DAC.
                                       // This method enables the chip select
                                       // Sets the SPI to have Polarity - 1
                                       // SMCLK as SCLK and Divided by /2
  
  UCB0BR0 = 0x10;                      // SCLK/16
  UCB0BR1 = 0;
  
  TI_CC_PowerupResetCCxxxx();          // Reset cc2500
  
  writeRFSettings();                   // Send RF settings to config regs

  TI_CC_SPIWriteReg( TI_CCxxx0_CHANNR,  8 );  // Set Your Own Channel Number
                                              // only AFTER writeRFSettings
  for( delay = 0; delay < 650; delay++ ) { }; // Let cc2500 finish setup
  
  // Configure ADC
  ADC10AE0  |= 0x01;                   // Enable chnl A0 = P2.0;

  ADC10CTL0 |= ADC10ON                 // Turn ON ADC10
            |  ADC10SHT_0              // samp-hold tim = 4 cyc;
            |  ADC10IE;                // Enable flag

  ADC10CTL1 |= ADC10SSEL_0             // ADC10CLK source = ADCOSC
            |  ADC10DIV_0              // ADC10CLK divider = 1
            |  INCH_0;                 // Select input = chnl A0 (default);
}

void main(void)
// ----------------------------------------------------------------------------
// Func:    Init I/O ports, wireless, and ADC with SetupAll
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{ 
  WDTCTL = WDTPW | WDTHOLD;        // Stop Watchdog Timer
  SetupAll();                      // Configure I/O pins, wireless, and ADC
  while ( 1 )
  {
    ADC10CTL0 |= ADC10SC | ENC;    // start next sample
    _BIS_SR ( LPM0_bits | GIE );   // Enter LPM1 w/ IRQs enab.
  }
}
// ======================================================================== EOF
