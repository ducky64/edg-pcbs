const int kSegmentCoding[10][7] = {
  // a  b  c  d  e  f  g
  {  1, 1, 1, 1, 1, 1, 0 },  // 0
  {  0, 1, 1, 0, 0, 0, 0 },  // 1
  {  1, 1, 0, 1, 1, 0, 1 },  // 2
  {  1, 1, 1, 1, 0, 0, 1 },  // 3
  {  0, 1, 1, 0, 0, 1, 1 },  // 4
  {  1, 0, 1, 1, 0, 1, 1 },  // 5
  {  1, 0, 1, 1, 1, 1, 1 },  // 6
  {  1, 1, 1, 0, 0, 0, 0 },  // 7
  {  1, 1, 1, 1, 1, 1, 1 },  // 8
  {  1, 1, 1, 1, 0, 1, 1 },  // 9
};
          
void setDigit(AddressableLight& it, Color& color, int digit, int offset, int ledsPerSegment) {
  const int* thisDigitSegments = kSegmentCoding[digit % 10];
  int ledIndex = offset;
  for (int segment=0; segment < 7; segment++) {
    for (int led=0; led < ledsPerSegment; led++) {
      if (thisDigitSegments[segment]) {
        it[ledIndex] = color;
      }
      ledIndex++;
    }
  }
}
