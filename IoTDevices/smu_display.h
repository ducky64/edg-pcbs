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
  int8_t digitsOffset = (numDigits + numDigitsDecimal) - strlen(digits);  // negative means overflow, positive is blank / zero digits

  char forcedChar = 0;  // if nonzero, all digits replaced with this
  if (isnan(value)) {
    forcedChar = '-';
  } else if (digitsOffset < 0) {  // if number exceeds length
    forcedChar = '+';
  }

  for (int8_t currentDigit = -1; currentDigit < (numDigits + numDigitsDecimal); currentDigit++) {
    char thisChar[2] = " ";
    if (forcedChar != 0) {
      thisChar[0] = forcedChar;
    } else {
      if (currentDigit < digitsOffset) {
        if (value < 0 && ((digitsOffset < numDigits && currentDigit == digitsOffset - 1) || (digitsOffset >= numDigits && currentDigit + 2 == numDigits))) {
          thisChar[0] = '-';
        } else if (currentDigit + 1 >= numDigits) {
          thisChar[0] = '0';
        } else {
          thisChar[0] = ' ';
        }
      } else {
        thisChar[0] = digits[currentDigit - digitsOffset];
      }
    }
    it.print(x, y, font, thisChar);
    x += width;

    if ((currentDigit + 1) != (numDigits + numDigitsDecimal)) {  // insert trailing markers like decimals and 3-digit-group separators
      if (currentDigit + 1 == numDigits) {
        it.print(x - 1, y, font, ".");
        x += width - 2;
      } else {
        if (currentDigit < numDigits && ((numDigits - currentDigit) % 3 == 1)) {  // insert a space every 3 digits
          x += 2;
        } else if (currentDigit > numDigits && ((currentDigit - numDigits) % 3 == 2)) {
          x += 2;
        }
      }
    }
  }
  return x;
}
