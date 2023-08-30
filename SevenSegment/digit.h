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
          
void setDigit(AddressableLight& it, Color& color, int digit, int offset, int ledsLit, int ledsPerSegment) {
  const int* thisDigitSegments = kSegmentCoding[digit % 10];
  for (int segment=0; segment < 7; segment++) {
    int segmentOffset = offset + segment * ledsPerSegment;
    for (int led=0; led < ledsLit; led++) {
      if (thisDigitSegments[segment]) {
        it[segmentOffset + led] = color;
      }
    }
  }
}
