uint32_t intpow10(uint8_t n) {
  static uint32_t lut[] = {
    1, 10, 100, 1000, 10000, 100000,
    1000000, 10000000, 100000000, 1000000000  //, 10000000000, 100000000000
  };
  return lut[n];
}

// Utility for drawing 
// underlineLoc is specified as 0 for the ones digit, 1 for the tens, and -1 for the 1/10s digit, and so on.
uint16_t drawValue(display::Display& it, int x, int y, font::Font* font, 
    uint8_t numDigits, uint8_t numDigitsDecimal, float value, int8_t underlineLoc = 127) {
  int width, baseline, dummy;
  font->measure("8", &width, &dummy, &baseline, &dummy);

  int32_t valueDecimal = value * intpow10(numDigitsDecimal + 1);
  if (valueDecimal > 0) {  // do rounding
    valueDecimal = (valueDecimal + 5) / 10;
  } else {
    valueDecimal = (valueDecimal - 5) / 10;
  }
  
  char digits[12] = {0};
  itoa(abs(valueDecimal), digits, 10);
  uint8_t digitsLen = strlen(digits);
  int8_t digitsOffset = (numDigits + numDigitsDecimal) - strlen(digits);  // negative means overflow, positive is blank / zero digits

  char forcedChar = 0;  // if nonzero, all digits replaced with this
  if (isnan(value)) {
    forcedChar = '-';
  } else if (digitsOffset < 0) {  // if number exceeds length
    forcedChar = '+';
  }

  // start at numDigits (which is one past the equivalent currentDigit) for the negative sign
  for (int8_t currentDigit = numDigits; currentDigit >= -numDigitsDecimal; currentDigit--) {
    int8_t digitsPos = (int8_t)digitsLen - (currentDigit + numDigitsDecimal) - 1;  // position within digits[]
    char thisChar[2] = " ";
    if (forcedChar != 0) {
      thisChar[0] = forcedChar;
    } else {
      if (digitsPos < 0) {
        if (value < 0 && ((digitsLen < numDigitsDecimal && digitsPos == -1) || (digitsLen >= numDigitsDecimal && digitsPos == -2))) {
          thisChar[0] = '-';
        } else if (currentDigit <= 0) {
          thisChar[0] = '0';
        } else {
          thisChar[0] = ' ';
        }
      } else {
        thisChar[0] = digits[digitsPos];
      }
    }

    if (underlineLoc == currentDigit) {
      it.filled_rectangle(x, y, width - 1, baseline - 1);
      it.print(x, y, font, COLOR_OFF, thisChar);
    } else {
      it.print(x, y, font, thisChar);
    }
    x += width;    

    if (currentDigit > -numDigitsDecimal) {  // insert trailing markers like decimals and 3-digit-group separators
      if (currentDigit == 0) {
        it.print(x - 1, y, font, ".");
        x += width - 2;
      } else if (currentDigit % 3 == 0) {
        x += 2;
      }
    }
  }

  return x;
}
