# acurite5n1arduino
Arduino sketch for receiving and decoding messages from Acurite 5n1 Weather Station (VN1TXC) using 433 MHz RX module 

Thanks to @helgew for tips on windspeed formula.

 * Works with 433 MHz superhet RX module, i.e., one with a crystal. Tested with RFM83C from anarduino. 
 * Tried also with cheaper regen module, i.e., one *without* a crystal, from ebay but too noisy, didn't work. YMMV 

## TODO:
 * Each raw byte has parity bit (MSB). Still occasionally get noisy packets which pass CRC but have junk values. Perhaps implementing byte-level parity check can help.
 * Output in more machine readable format, e.g., csv or json. Add simple python script which can tail serial port, consume and parse events, add timestamp, and log. This could itself be consumed/piped otherwise sent to other weather logging/reporting systems.
