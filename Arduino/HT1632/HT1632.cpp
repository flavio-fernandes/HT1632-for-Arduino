#include "HT1632.h"

#ifdef RASPBERRY_PI

#include <stdlib.h>
#include <time.h>
#include <math.h>       /* ceil */
#include <wiringPi.h>

#pragma GCC diagnostic ignored "-Wchar-subscripts"

#else // ifdef RASPBERRY_PI

#if (ARDUINO >= 100)
  #include <Arduino.h>
#else
  #include <WProgram.h>
#endif

#endif // ifdef RASPBERRY_PI

/*
 * HIGH LEVEL FUNCTIONS
 * Functions that perform advanced tasks using lower-level
 * functions go here:
 */

void HT1632Class::drawText(const char text [], int x, int y, const char font [], const char font_width [], char font_height, int font_glyph_step, char gutter_space) {
  int curr_x = x;
  char i = 0;
  char currchar;
  
  // Check if string is within y-bounds
  if(y + font_height < 0 || y >= COM_SIZE)
    return;
  
  while(true){  
    if(text[i] == '\0')
      return;
    
    currchar = text[i] - 32;
    if(currchar >= 65 && currchar <= 90) // If character is lower-case, automatically make it upper-case
      currchar -= 32; // Make this character uppercase.
    if(currchar < 0 || currchar >= 64) { // If out of bounds, skip
      ++i;
      continue; // Skip this character.
    }
    // Check to see if character is not too far right.
    if(curr_x >= OUT_SIZE)
      break; // Stop rendering - all other characters are no longer within the screen 
    
    // Check to see if character is not too far left.
    if(curr_x + font_width[currchar] + gutter_space >= 0){
      drawImage(font, font_width[currchar], font_height, curr_x, y,  currchar*font_glyph_step);
      
      // Draw the gutter space
      for(char j = 0; j < gutter_space; ++j)
        drawImage(font, 1, font_height, curr_x + font_width[currchar] + j, y, 0);
      
    }
    
    curr_x += font_width[currchar] + gutter_space;
    ++i;
  }
}

// Gives you the width, in columns, of a particular string.
int HT1632Class::getTextWidth(const char text [], const char font_width [], char font_height, char gutter_space) {
  int wd = 0;
  char i = 0;
  char currchar;
  
  while(true){  
    if(text[i] == '\0')
      return wd - gutter_space;
      
    currchar = text[i] - 32;
    if(currchar >= 65 && currchar <= 90) // If character is lower-case, automatically make it upper-case
      currchar -= 32; // Make this character uppercase.
    if(currchar < 0 || currchar >= 64) { // If out of bounds, skip
      ++i;
      continue; // Skip this character.
    }
    wd += font_width[currchar] + gutter_space;
    ++i;
  }
}

/*
 * MID LEVEL FUNCTIONS
 * Functions that handle internal memory, initialize the hardware
 * and perform the rendering go here:
 */

#ifdef BICOLOR_MATRIX

void HT1632Class::begin(int pinCS, int pinWR, int pinDATA, int pinCLK) {
  _pinForCS = pinCS;
  _pinWR = pinWR;
  _pinDATA = pinDATA;
  _pinCLK = pinCLK;

  int i=0;
  
  // Allocate new memory for mem (including secondary)
  for(i=0; i < MAX_BOARDS; ++i) mem[i] = (char *) malloc(ADDR_SPACE_SIZE);

  pinMode(_pinForCS, OUTPUT);
  pinMode(_pinWR, OUTPUT);
  pinMode(_pinDATA, OUTPUT);
  pinMode(_pinCLK, OUTPUT);
  
  // Each 8-bit mem array element stores data in the 4 least significant bits,
  //   and meta-data in the 4 most significant bits. Use bitmasking to read/write
  //   the meta-data.

  // Send configuration to chip:
  // This configuration is from the HT1632 datasheet, with one modification:
  //   The RC_MASTER_MODE command is not sent to the master. Since acting as
  //   the RC Master is the default behaviour, this is not needed. Sending
  //   this command causes problems in HT1632C (note the C at the end) chips. 

  select(0);

  // Send Master commands
  for(i=1; i <= NUM_ACTIVE_CHIPS; ++i) {
    select(i);  // 1 based
    writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
  
    writeCommand(HT1632_CMD_SYSDIS); // Turn off system oscillator
    writeCommand(HT1632_CMD_COMS00); // 16*32, PMOS drivers
    //    writeCommand(HT1632_CMD_MSTMD);  // Master Mode (!!!!)
    writeCommand(HT1632_CMD_RCCLK);  // Master Mode, external clock
    writeCommand(HT1632_CMD_SYSEN); // Turn on system
    writeCommand(HT1632_CMD_LEDON); // Turn on LED duty cycle generator
    writeCommand(HT1632_CMD_PWM(16)); // PWM 16/16 duty
    writeCommand(HT1632_CMD_BLOFF); // Blink off
    select(0);
  }
  
  for(i=0; i < MAX_BOARDS; ++i) {
    _globalNeedsRewriting[i] = false;
    drawTarget(i);
    clear();
    render(); // Perform the initial rendering
  }

  // Set drawTarget to default board.
  drawTarget(0);

  digitalWrite(_pinWR, LOW);
}

void HT1632Class::initialize(int pinWR, int pinDATA) {
  // noop, all init work was done above
}

#else // BICOLOR_MATRIX

void HT1632Class::begin(int pinCS1, int pinWR, int pinDATA) {
  _numActivePins = 1;
  _pinCS[0] = pinCS1;
  initialize(pinWR, pinDATA);
}
void HT1632Class::begin(int pinCS1, int pinCS2, int pinWR,   int pinDATA) {
  _numActivePins = 2;
  _pinCS[0] = pinCS1;
  _pinCS[1] = pinCS2;
  initialize(pinWR, pinDATA);
}
void HT1632Class::begin(int pinCS1, int pinCS2, int pinCS3,  int pinWR,   int pinDATA) {
  _numActivePins = 3;
  _pinCS[0] = pinCS1;
  _pinCS[1] = pinCS2;
  _pinCS[2] = pinCS3;
  initialize(pinWR, pinDATA);
}
void HT1632Class::begin(int pinCS1, int pinCS2, int pinCS3,  int pinCS4,  int pinWR,   int pinDATA) {
  _numActivePins = 4;
  _pinCS[0] = pinCS1;
  _pinCS[1] = pinCS2;
  _pinCS[2] = pinCS3;
  _pinCS[3] = pinCS4;
  initialize(pinWR, pinDATA);
}

void HT1632Class::initialize(int pinWR, int pinDATA) {
  _pinWR = pinWR;
  _pinDATA = pinDATA;
  
  for(int i=0; i<_numActivePins; ++i){
    pinMode(_pinCS[i], OUTPUT);
    // Allocate new memory for mem
    mem[i] = (char *)malloc(ADDR_SPACE_SIZE);
    drawTarget(i);
    clear(); // Clean out mem
  }
  pinMode(_pinWR, OUTPUT);
  pinMode(_pinDATA, OUTPUT);
  
  select();
  
  mem[4] = (char *)malloc(ADDR_SPACE_SIZE);
  // Each 8-bit mem array element stores data in the 4 least significant bits,
  //   and meta-data in the 4 most significant bits. Use bitmasking to read/write
  //   the meta-data.
  drawTarget(4);
  clear();
  // Clean out memory
  int i=0;
  
  
  // Send configuration to chip:
  // This configuration is from the HT1632 datasheet, with one modification:
  //   The RC_MASTER_MODE command is not sent to the master. Since acting as
  //   the RC Master is the default behaviour, this is not needed. Sending
  //   this command causes problems in HT1632C (note the C at the end) chips. 
  
  // Send Master commands
  
  select(0b1111); // Assume that board 1 is the master.
  writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
  
  writeCommand(HT1632_CMD_SYSDIS); // Turn off system oscillator
   // N-MOS or P-MOS open drain output and 8 or 16 common option
#if   USE_NMOS == 1 && COM_SIZE == 8
  writeCommand(HT1632_CMD_COMS00);
#elif USE_NMOS == 1 && COM_SIZE == 16
  writeCommand(HT1632_CMD_COMS01);
#elif USE_NMOS == 0 && COM_SIZE == 8
  writeCommand(HT1632_CMD_COMS10);
#elif USE_NMOS == 0 && COM_SIZE == 16
  writeCommand(HT1632_CMD_COMS11);
#else
#error Invalid USE_NMOS or COM_SIZE values! Change the values in HT1632.h.
#endif
  
  if(false && _numActivePins > 1){
    select(0b0001); // 
    writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode   
    writeCommand(HT1632_CMD_RCCLK); // Switch system to MASTER mode    
    select(0b1110); // All other boards are slaves
    writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
    
    writeCommand(HT1632_CMD_MSTMD); // Switch system to SLAVE mode.
    // The use of the MSTMD command to switch to slave is explained here:
    // http://forums.parallax.com/showthread.php?117423-Electronics-I-O-%28outputs-to-common-anode-RGB-LED-matrix%29-question/page4
    
    
    select(0b1111);
    writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
  }

  writeCommand(HT1632_CMD_SYSEN); //Turn on system
  writeCommand(HT1632_CMD_LEDON); // Turn on LED duty cycle generator
  writeCommand(HT1632_CMD_PWM(16)); // PWM 16/16 duty
  writeCommand(HT1632_CMD_BLOFF); // Be sure blink is off.
  
  select();
   
  
  for(int i=0; i<_numActivePins; ++i) {
    drawTarget(i);
    _globalNeedsRewriting[i] = true;
    clear();
    // Perform the initial rendering
    render();
  }
  // Set drawTarget to default board.
  drawTarget(0);
}

#endif // BICOLOR_MATRIX

void HT1632Class::setPixel(int loc_x, int loc_y, bool datum) {
  if (datum) {
    mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] | (1 << (loc_y % 4))) | MASK_NEEDS_REWRITING;
  }
  else {
    mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & ~(1 << (loc_y % 4))) | MASK_NEEDS_REWRITING;
  }
}

void HT1632Class::drawTarget(char targetBuffer) {
#ifdef BICOLOR_MATRIX
  if (targetBuffer >= 0 && targetBuffer < MAX_BOARDS) _tgtBuffer = targetBuffer;
#else // BICOLOR_MATRIX
  if(targetBuffer == 0x04 || (targetBuffer >= 0 && targetBuffer < _numActivePins))  
    _tgtBuffer = targetBuffer;
#endif // BICOLOR_MATRIX
}

void HT1632Class::drawImage(const char * img, char width, char height, int x, int y, int offset){
  char mask;
  
  // Sanity checks
  if(y + height < 0 || x + width < 0 || y > COM_SIZE || x > OUT_SIZE)
    return;
  // After looking at the rest of this function, you may need one.
  
  // Copying Engine.
  // You are only expected to understand this if it does not work right. ;)
  for(char i=0; i<width; ++i) {
    char carryover_y = 0; // Simply a copy of the last 4-bit word of img.
    char carryover_num = (y - (y & ~ 3)); // Number of digits carried over
    bool carryover_valid = false; // If true, there is data to be carried over.
    
    char loc_x = i + x;
    if(loc_x < 0 || loc_x >= OUT_SIZE) // Skip this column if it is out of range.
      continue;
    for(char j=0; j < (carryover_valid ? (height+4):height) ; j+=4) {
      char loc_y = j + y;
      if(loc_y <= -4 || loc_y >= COM_SIZE) // Skip this row if it is out of range.
        continue;
      // Direct copying possible when render is on boundaries.
      // The bit manipulation here is designed to copy from img only the relevant sections.
      
      // This mask is only not used when emptying the cache (for copying to non-4-bit aligned spaces)
     
      //if(j<height)
      //  mask = (height-loc_y >= 4)?0b00001111:(0b00001111 >> (4-(height-j))) & 0b00001111; // Mask bottom
        
      if(loc_y % 4 == 0) {
	  const int shiftBottom = 4-(height-j);
	  mask = (height-loc_y >= 4 || shiftBottom <= 0) ? 0b00001111 : (0b00001111 >> shiftBottom) & 0b00001111; // Mask bottom
#ifdef RASPBERRY_PI
          mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | (img[(int)ceil((float)height/4.0f)*i + j/4 + offset] & mask) | MASK_NEEDS_REWRITING;
#else // ifdef RASPBERRY_PI 
          mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | (pgm_read_byte(&img[(int)ceil((float)height/4.0f)*i + j/4 + offset]) & mask) | MASK_NEEDS_REWRITING;
#endif // ifdef RASPBERRY_PI 
      } else {
        // If carryover_valid is NOT true, then this is the first set to be copied.
        //   If loc_y > 0, preserve the contents of the pixels above, copy to mem, and then copy remaining
        //   data to the carry over buffer. If y loc_y < 0, just copy data to carry over buffer. 
        //   It is expected that this section is only reached when j == 0.
        // COPY START
        if(!carryover_valid) { 
          if(loc_y > 0) {
	    const int shiftBottom = 4-(height-j);
	    mask = (height-loc_y >= 4 || shiftBottom <= 0) ? 0b00001111 : (0b00001111 >> shiftBottom) & 0b00001111; // Mask bottom
            mask = (0b00001111 << carryover_num) & mask; // Mask top
#ifdef RASPBERRY_PI 
            mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | ((img[(int)ceil((float)height/4.0f)*i + j/4 + offset] << carryover_num) & mask) | MASK_NEEDS_REWRITING;
#else  // ifdef RASPBERRY_PI 
            mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | ((pgm_read_byte(&img[(int)ceil((float)height/4.0f)*i + j/4 + offset]) << carryover_num) & mask) | MASK_NEEDS_REWRITING;
#endif  // ifdef RASPBERRY_PI 
          }
          carryover_valid = true;
        } else {
          // COPY END
          if(j >= height) {
            // Its writing one line past the end.
            // Use this line to get rid of the final carry-over.
            mask = (0b00001111 >> (4 - carryover_num)) & 0b00001111; // Mask bottom
            mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | (carryover_y >> (4 - carryover_num) & mask) | MASK_NEEDS_REWRITING;
          // COPY MIDDLE  
          } else {
            // There is data in the carry-over buffer. Copy that data and the values from the current cell into mem.
            // The inclusion of a carryover_num term is to account for the presence of the carryover data  when calculating the bottom clipping.
	    const int shiftBottom = 4-(height+carryover_num-j);
	    mask = (height-loc_y >= 4 || shiftBottom <= 0) ? 0b00001111 : (0b00001111 >> shiftBottom) & 0b00001111; // Mask bottom
#ifdef RASPBERRY_PI 
            mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | ((img[(int)ceil((float)height/4.0f)*i + j/4 + offset] << carryover_num) & mask) | (carryover_y >> (4 - carryover_num) & mask) | MASK_NEEDS_REWRITING;
#else // ifdef RASPBERRY_PI 
            mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] = (mem[_tgtBuffer][GET_ADDR_FROM_X_Y(loc_x,loc_y)] & (~mask) & 0b00001111) | ((pgm_read_byte(&img[(int)ceil((float)height/4.0f)*i + j/4 + offset]) << carryover_num) & mask) | (carryover_y >> (4 - carryover_num) & mask) | MASK_NEEDS_REWRITING;
#endif // ifdef RASPBERRY_PI 
          }
        }
#ifdef RASPBERRY_PI
        carryover_y = img[(int)ceil((float)height/4.0f)*i + j/4 + offset];
#else // ifdef RASPBERRY_PI 
        carryover_y = pgm_read_byte(&img[(int)ceil((float)height/4.0f)*i + j/4 + offset]);
#endif // ifdef RASPBERRY_PI 
      }
    }
  }
}

void HT1632Class::clear(){
  // Note: Must use int below, because in BICOLOR screens, addr space is greater than 255
  for(int i=0; i < ADDR_SPACE_SIZE; ++i) mem[_tgtBuffer][i] = 0x00 | MASK_NEEDS_REWRITING; // Needs to be redrawn 
}

// Draw the contents of map to screen, for memory addresses that have the needsRedrawing flag
void HT1632Class::render() {
#ifdef BICOLOR_MATRIX
  if(_tgtBuffer >= BUFFER_SECONDARY || _tgtBuffer < 0) return;
  
  // char selectionmask = _tgtBuffer + 1;
  char nChip;
  char nChipOpen = -1;                   // Automatically compact sequential writes.
  char chipBasedAddress;
  const int colorOffset = _tgtBuffer * 32;      // Color (aka board) memory offset in chip 
  
  for(int i=0; i < ADDR_SPACE_SIZE; ++i) {
    if (_globalNeedsRewriting[_tgtBuffer] || 
	(mem[_tgtBuffer][i] & MASK_NEEDS_REWRITING)) {  // Does this memory chunk need to be written to?
      nChip = (i / 32) + 1;  // calculate nChip we will need to talk to (1 based!)
      if ( nChipOpen != nChip ) {                      // If necessary, open the writing session by:
	chipBasedAddress = i % 32;
        select(nChip);       //   Selecting the chip
        writeData(HT1632_ID_WR, HT1632_ID_LEN);
        writeData(chipBasedAddress + colorOffset, HT1632_ADDR_LEN);   //   Selecting the memory address
        nChipOpen = nChip;
      }
      writeDataRev(mem[_tgtBuffer][i], HT1632_WORD_LEN); // Write the data in reverse.
    } else {                               // If a previous sequential write session is open, close it.
      if (nChipOpen != -1) { select(0); nChipOpen = -1; }
    }
  }
  if (nChipOpen != -1) { // Close the stream at the end
    select(0);
    // nChipOpen = -1;
  }

#else // BICOLOR_MATRIX

  if(_tgtBuffer >= _numActivePins || _tgtBuffer < 0)
    return;
  
  char selectionmask = 0b0001 << _tgtBuffer;
  
  bool isOpen = false;                   // Automatically compact sequential writes.
  for(int i=0; i<ADDR_SPACE_SIZE; ++i)
    if(_globalNeedsRewriting[_tgtBuffer] || (mem[_tgtBuffer][i] & MASK_NEEDS_REWRITING)) {  // Does this memory chunk need to be written to?
      if(!isOpen) {                      // If necessary, open the writing session by:
        select(selectionmask);           //   Selecting the chip
        writeData(HT1632_ID_WR, HT1632_ID_LEN);
        writeData(i, HT1632_ADDR_LEN);   //   Selecting the memory address
        isOpen = true;
      }
      writeDataRev(mem[_tgtBuffer][i], HT1632_WORD_LEN); // Write the data in reverse.
    } else                               // If a previous sequential write session is open, close it.
      if(isOpen) {
        select();
        isOpen = false;
      }

  if(isOpen) {                           // Close the stream at the end
    select();
    isOpen = false;
  }

#endif // BICOLOR_MATRIX

  _globalNeedsRewriting[_tgtBuffer] = false;
}

// Set the brightness to an integer level between 1 and 16 (inclusive).
// Uses the PWM feature to set the brightness.
void HT1632Class::setBrightness(char brightness, char selectionmask) {
#ifdef BICOLOR_MATRIX
  // NOTE: selectionmask is not really useful in BICOLOR boards, because
  //       it is not granular enough to the end user. Same applies to blink.
  //       So we just did not bother with it in here...
  for(int i=1; i <= NUM_ACTIVE_CHIPS; ++i) {
    select(i);  // 1 based!
    writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
    writeCommand(HT1632_CMD_PWM(brightness));   // Set brightness
  }
  select(0);
#else // BICOLOR_MATRIX
  if(selectionmask == 0b00010000) {
    if(_tgtBuffer < _numActivePins)
      selectionmask = 0b0001 << _tgtBuffer;
    else
      return;
  }
  
  select(selectionmask); 
  writeData(HT1632_ID_CMD, HT1632_ID_LEN);    // Command mode
  writeCommand(HT1632_CMD_PWM(brightness));   // Set brightness
  select();
#endif // BICOLOR_MATRIX
}

void HT1632Class::transition(char mode, int time){
#ifdef BICOLOR_MATRIX
  if(_tgtBuffer >= BUFFER_SECONDARY || _tgtBuffer < 0) return;
#else // BICOLOR_MATRIX
  if(_tgtBuffer >= _numActivePins || _tgtBuffer < 0)
    return;
#endif // BICOLOR_MATRIX
  
  switch(mode) {
    case TRANSITION_BUFFER_SWAP:
      {
        char * tmp = mem[_tgtBuffer];
        mem[_tgtBuffer] = mem[BUFFER_SECONDARY];
        mem[BUFFER_SECONDARY] = tmp;
        _globalNeedsRewriting[_tgtBuffer] = true;
      }
      break;
    case TRANSITION_NONE:
      for(int i=0; i < ADDR_SPACE_SIZE; ++i)
        mem[_tgtBuffer][i] = mem[BUFFER_SECONDARY][i]; // Needs to be redrawn 
      _globalNeedsRewriting[_tgtBuffer] = true;
      break;
    case TRANSITION_FADE:
      time /= 32;
      for(int i = 15; i > 0; --i) {
        setBrightness(i);
        delay(time);
      }
      clear();
      render();
      delay(time);
      transition(TRANSITION_BUFFER_SWAP);
      render();
      delay(time);
      for(int i = 2; i <= 16; ++i) {
        setBrightness(i);
        delay(time);
      }
      break;
  }
  
}


/*
 * LOWER LEVEL FUNCTIONS
 * Functions that directly talk to hardware go here:
 */
 
void HT1632Class::writeCommand(char data) {
  writeData(data, HT1632_CMD_LEN);
  writeSingleBit();
} 
// Integer write to display. Used to write commands/addresses.
// PRECONDITION: WR is LOW
void HT1632Class::writeData(char data, char len) {
  for(int j=len-1, t = 1 << (len - 1); j>=0; --j, t >>= 1){
    // Set the DATA pin to the correct state
    digitalWrite(_pinDATA, ((data & t) == 0)?LOW:HIGH);
    NOP(); // Delay 
    // Raise the WR momentarily to allow the device to capture the data
    digitalWrite(_pinWR, HIGH);
    NOP(); // Delay
    // Lower it again, in preparation for the next cycle.
    digitalWrite(_pinWR, LOW);
  }
}
// REVERSED Integer write to display. Used to write cell values.
// PRECONDITION: WR is LOW
void HT1632Class::writeDataRev(char data, char len) {
  for(int j=0; j<len; ++j){
    // Set the DATA pin to the correct state
    digitalWrite(_pinDATA, data & 1);
    NOP(); // Delay
    // Raise the WR momentarily to allow the device to capture the data
    digitalWrite(_pinWR, HIGH);
    NOP(); // Delay
    // Lower it again, in preparation for the next cycle.
    digitalWrite(_pinWR, LOW);
    data >>= 1;
  }
}
// Write single bit to display, used as padding between commands.
// PRECONDITION: WR is LOW
void HT1632Class::writeSingleBit() {
  // Set the DATA pin to the correct state
  digitalWrite(_pinDATA, LOW);
  NOP(); // Delay
  // Raise the WR momentarily to allow the device to capture the data
  digitalWrite(_pinWR, HIGH);
  NOP(); // Delay
  // Lower it again, in preparation for the next cycle.
  digitalWrite(_pinWR, LOW);
}

#ifdef BICOLOR_MATRIX

//Output a clock pulse
static inline void outputCLK_Pulse(char _pinCLK) { digitalWrite(_pinCLK, HIGH); digitalWrite(_pinCLK, LOW); }

// Choose a chip. This function sets the correct CS line to LOW, and the rest to HIGH
// Call the function with no arguments to deselect all chips.
void HT1632Class::select(char mask) {
  char tmp = 0;

  if (mask < 0) { // Enable all HT1632C
    digitalWrite(_pinForCS, LOW);
    for (tmp = 0; tmp < NUM_ACTIVE_CHIPS; tmp++) outputCLK_Pulse(_pinCLK);
  } else if (mask == 0) { //Disable all HT1632Cs
    digitalWrite(_pinForCS, HIGH);
    for(tmp = 0; tmp < NUM_ACTIVE_CHIPS; tmp++) outputCLK_Pulse(_pinCLK);
  } else {
    digitalWrite(_pinForCS, HIGH);
    for(tmp = 0; tmp < NUM_ACTIVE_CHIPS; tmp++) outputCLK_Pulse(_pinCLK);
    digitalWrite(_pinForCS, LOW);
    outputCLK_Pulse(_pinCLK);
    digitalWrite(_pinForCS, HIGH);
    for(tmp = 1 ; tmp < mask; tmp++) outputCLK_Pulse(_pinCLK);
  }
}
void HT1632Class::select() {
  select(0);
}

#else // BICOLOR_MATRIX

// Choose a chip. This function sets the correct CS line to LOW, and the rest to HIGH
// Call the function with no arguments to deselect all chips.
// Call the function with a bitmask (0b4321) to select specific chips. 0b1111 selects all. 
void HT1632Class::select(char mask) {
  for(int i=0, t=1; i<_numActivePins; ++i, t <<= 1){
    digitalWrite(_pinCS[i], (t & mask)?LOW:HIGH);
    /*Serial.write(48+_pinCS[i]);
    Serial.write((t & mask)?"LOW":"HIGH");
    Serial.write('\n');
  */}
  //Serial.write('\n');
}
void HT1632Class::select() {
  for(int i=0; i<_numActivePins; ++i)
    digitalWrite(_pinCS[i], HIGH);
}

#endif // BICOLOR_MATRIX

/*
 * HELPER FUNCTIONS
 * "Would you like some fries with that?"
 */

void HT1632Class::recursiveWriteUInt (int inp) {
#ifndef RASPBERRY_PI
  if(inp <= 0) return;
  int rd = inp % 10;
  recursiveWriteUInt(inp/10);
  Serial.write(48+rd);
#endif // ifndef RASPBERRY_PI
}

void HT1632Class::writeInt (int inp) {
#ifndef RASPBERRY_PI
  if(inp == 0)
    Serial.write('0');
  else
    if (inp < 0){
      Serial.write('-');
      recursiveWriteUInt(-inp);
    } else 
      recursiveWriteUInt(inp);
#endif // ifndef RASPBERRY_PI
}

HT1632Class HT1632;

