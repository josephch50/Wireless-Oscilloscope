// ======================================================================== BOF
#include "msp430x22x4.h"         // Large MSP header
#include "stdint.h"              // Standard Integers for MSP
#include "wireless-v2.h"         // Include the wireless header file

typedef union                    // Control & Data word type for DAC
{
  uint16_t u16;                  // 16-bits for easy arithmetic on full word
  uint8_t  u8[2];                // 08-bits for easy write to 8-bit TX-BUF reg.
} dacWord_t;

volatile dacWord_t dacWordOut;
uint8_t rxPkt[2] = {0x00, 0x00};

//------------------------------------------------------------------------------
// Func:  Packet Received ISR:  triggered by falling edge of GDO0.
//        Parses pkt & transfers over SPI to DAC.
// Args:  None
// Retn:  None
//------------------------------------------------------------------------------
#pragma vector=PORT2_VECTOR        // cc2500 pin GDO0 is hardwired to P2.6
__interrupt void PktRxedISR(void)
{
  static uint8_t len = 2;          // Packet Len = 2 bytes (data only)
  uint8_t status[2];               // Buffer to store pkt status bytes
                 
  if( TI_CC_GDO0_PxIFG & TI_CC_GDO0_PIN )    // chk GDO0 bit of P2 IFG Reg
  {
    RFReceivePacket(rxPkt, &len, status);  // Get packet from cc2500
  }
  
  dacWordOut.u8[1] = rxPkt[1];  // Save the data from the transmission
  dacWordOut.u8[0] = rxPkt[0];  // LSB was first then MSB
  
  dacWordOut.u16 = dacWordOut.u16 << 2; // 10-bit to 12-bit scaling
  
  TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;  // Reset GDO0 IRQ flag
  TI_CC_SPIStrobe(TI_CCxxx0_SIDLE);  // Set cc2500 to idle mode.
  TI_CC_SPIStrobe(TI_CCxxx0_SRX);    // Set cc2500 to RX mode.
                                     // AutoCal @ IDLE to RX Transition                                     

  UCB0CTL0 &= ~(UCCKPH);             // Change to be in the same phase 
                                     // as the DAC (from being the phase
                                     // of the cc2500)
  
  while ( UCB0STAT & UCBUSY ){};     // Wait til SPI is idle (should be now)
  
  P4OUT &= ~0x08;                    // Assert SS (active low)
                                     
  while ( !(IFG2 & UCB0TXIFG) ){};   // Wait til TXBUF is ready (empty)
  UCB0TXBUF = dacWordOut.u8[1];      // Tx next byte (TXIFG auto-clr)
  
  while ( !(IFG2 & UCB0TXIFG) ){};   // Wait til TXBUF is ready (empty)
  UCB0TXBUF = dacWordOut.u8[0];      // Tx next byte (TXIFG auto-clr)
  
  while ( UCB0STAT & UCBUSY ){};     // Wait til ALL bits shift out SPI
  P4OUT |= 0x08;                     // De-assert SS => End of stream
  UCB0CTL0 |= UCCKPH;                // Change back to polarity of cc2500
}

void SetupAll (void)
// ----------------------------------------------------------------------------
// Func:    Initialize Button & LED ports, and ports for I/O on TA1 Capture.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{
  volatile uint16_t delay;
  for( delay = 0; delay < 650; delay++ ) { }; // Empirical: cc2500 Power Up

  // Set up clock system
  BCSCTL1 = CALBC1_8MHZ;                // set DCO = 8MHz
  DCOCTL  = CALDCO_8MHZ;                // set DCO = 8MHz

  // Wireless Initialization
  P2SEL = 0;                            // P2.6, P2.7 = GDO0, GDO2 (GPIO)

  TI_CC_SPISetup();                     // Initialize SPI port for both the
                                        // SPI communication b/w cc2500 and
                                        // DAC.
                                        // This method enables the chip select
                                        // Sets the SPI to have Polarity - 1
                                        // SMCLK as SCLK and Divided by /2
  
  UCB0BR0 = 0x10;                       // UCLK/2
  UCB0BR1 = 0;

  TI_CC_PowerupResetCCxxxx();           // Reset cc2500
  writeRFSettings();                    // Send RF settings to config regs

  TI_CC_GDO0_PxIES |=  TI_CC_GDO0_PIN;  // IRQ on GDO0 fall. edge (end pkt)
  TI_CC_GDO0_PxIFG &= ~TI_CC_GDO0_PIN;  // Clear  GDO0 IRQ flag
  TI_CC_GDO0_PxIE  |=  TI_CC_GDO0_PIN;  // Enable GDO0 IRQ
  
  TI_CC_SPIStrobe(TI_CCxxx0_SRX);       // Init. cc2500 in RX mode.
  TI_CC_SPIWriteReg(TI_CCxxx0_CHANNR, 8);     // Set Your Own Channel Number
                                              // AFTER writeRFSettings

  for( delay = 0; delay < 650; delay++ ) { }; // Empirical: cc2500 Power Up
  
  // Slave Select for transmitting to DAC
  P4SEL &= ~0x08;              // P4.3 Select as GPIO Alt Function
  P4DIR |= 0x08;               // P4.3 Select as an output
  P4OUT |= 0x08;               // P4.3 Set to be High (SS is active low)
}

void main(void)
// ----------------------------------------------------------------------------
// Func:    Init I/O ports & IRQs, Config. Timer TA Chnl 1, enter LowPwr Mode.
// Args:    None
// Retn:    None
// ----------------------------------------------------------------------------
{
  WDTCTL = WDTPW | WDTHOLD;          // Stop Watchdog Timer
  SetupAll();                        // Configure I/O Pins

  _BIS_SR ( LPM0_bits | GIE );       // Enter LPM1 w/ IRQs enab.
}
// ======================================================================== EOF
