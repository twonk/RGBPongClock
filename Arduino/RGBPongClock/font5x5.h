#include <avr/pgmspace.h>
// copied from http://heim.ifi.uio.no/haakoh/avr/a
const int fnt_count = 37;
unsigned char PROGMEM font5x5[38][5] = {
	{0, 0, 0, 0, 0}, // space!
  {0x7c,0x44,0x44,0x7c,0x44},  //A  
  {0x7c,0x44,0x78,0x44,0x7c},  
  {0x7c,0x40,0x40,0x40,0x7c},  
  {0x78,0x44,0x44,0x44,0x78},  //d
  {0x7c,0x40,0x78,0x40,0x7c},
  {0x7c,0x40,0x70,0x40,0x40},
  {0x7c,0x40,0x4c,0x44,0x7c},//G
  {0x44,0x44,0x7c,0x44,0x44},
  {0x7c,0x10,0x10,0x10,0x7c},
  {0x0c,0x04,0x04,0x44,0x7c},//J
  {0x44,0x48,0x70,0x48,0x44},
  {0x40,0x40,0x40,0x40,0x7c},
  {0x44,0x6c,0x54,0x44,0x44},//M
  {0x44,0x64,0x54,0x4c,0x44},
  {0x38,0x44,0x44,0x44,0x38},  
  {0x78,0x44,0x78,0x40,0x40},	//P	  
  {0x7c,0x44,0x44,0x7c,0x10},
  {0x78,0x44,0x78,0x44,0x44},
  {0x7c,0x40,0x7c,0x04,0x7c},//s
  {0x7c,0x10,0x10,0x10,0x10},		  
  {0x44,0x44,0x44,0x44,0x7c},
  {0x44,0x44,0x28,0x28,0x10},//V
  {0x44,0x44,0x54,0x54,0x28},
  {0x44,0x28,0x10,0x28,0x44},
  {0x44,0x44,0x28,0x10,0x10},//Y
  {0x7c,0x08,0x10,0x20,0x7c},
  
  {0x7c,0x4c,0x54,0x64,0x7c},//0
  {0x10,0x30,0x10,0x10,0x38},
  {0x78,0x04,0x38,0x40,0x7c},//2
  {0x7c,0x04,0x38,0x04,0x7c},
  {0x40,0x40,0x50,0x7c,0x10},//4
  {0x7c,0x40,0x78,0x04,0x78},
  {0x7c,0x40,0x7c,0x44,0x7c},//6
  {0x7c,0x04,0x08,0x10,0x10},
  {0x7c,0x44,0x7c,0x44,0x7c},
  {0x7c,0x44,0x7c,0x04,0x7c},//9
	{0x7c,0x7c,0x7c,0x7c,0x7c}// block


};
