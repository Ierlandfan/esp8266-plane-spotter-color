/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://blog.squix.org


*/
// Go to settings to change important parameters
#include "settings.h"

#include "PlaneSpotter.h"
#include <SPI.h>

PlaneSpotter::PlaneSpotter(TFT_eSPI* tft, GeoMap* geoMap) {
  tft_ = tft;
  geoMap_ = geoMap;
}

void PlaneSpotter::copyProgmemToSpiffs(const uint8_t *data, unsigned int length, String filename) {
  fs::File f = SPIFFS.open(filename, "w+");
  uint8_t c;
  for(int i = 0; i < length; i++) {
    c = pgm_read_byte(data + i);
    f.write(c);
  }
  f.close();
}


void PlaneSpotter::drawSPIFFSJpeg(String filename, int32_t xpos, int32_t ypos) {
  Serial.println(filename);
  JpegDec.decodeFile(filename);
  //jpegInfo();
  renderJPEG(xpos, ypos);
}

void PlaneSpotter::renderJPEG(int32_t xpos, int32_t ypos) {

  uint8_t  *pImg;
  uint32_t mcu_w = JpegDec.MCUWidth;
  uint32_t mcu_h = JpegDec.MCUHeight;
  int32_t max_x = JpegDec.width;
  int32_t max_y = JpegDec.height;

  uint32_t min_w = min(mcu_w, max_x % mcu_w);
  uint32_t min_h = min(mcu_h, max_y % mcu_h);

  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  uint32_t drawTime = millis();

  max_x += xpos;
  max_y += ypos;

  while( JpegDec.readSwappedBytes()){
    
    pImg = (uint8_t*)JpegDec.pImage;
    int32_t mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int32_t mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    uint32_t mcu_pixels = win_w * win_h * 2;

    if ( ( mcu_x + win_w) <= tft_->width() && ( mcu_y + win_h) <= tft_->height()){
      tft_->setWindow(mcu_x, mcu_y, mcu_x + win_w - 1, mcu_y + win_h - 1);
      tft_->pushColors(pImg, mcu_pixels);    // pushColors via 64 byte SPI port buffer
    }

    else if( ( mcu_y + mcu_h) >= tft_->height()) JpegDec.abort();
  
  }
}

void PlaneSpotter::drawAircraftHistory(Aircraft aircraft, AircraftHistory history) {
    Coordinates lastCoordinates;
    lastCoordinates.lat = aircraft.lat;
    lastCoordinates.lon = aircraft.lon;
    for (int j = 0; j < min(history.counter, MAX_HISTORY); j++) {

      AircraftPosition position = history.positions[j];
      Coordinates coordinates = position.coordinates;
      CoordinatesPixel p1 = geoMap_->convertToPixel(coordinates);

      CoordinatesPixel p2 = geoMap_->convertToPixel(lastCoordinates);
      uint16_t color = heightPalette_[min(position.altitude / 4000, 9)];
      tft_->drawLine(p1.x, p1.y, p2.x, p2.y, color);
      tft_->drawLine(p1.x+1, p1.y+1, p2.x+1, p2.y+1, color);

      lastCoordinates = coordinates;
      // Serial.println(String(j) + ". " + String(historyIndex) + ". " + String(coordinates.lat) + ", " + String(coordinates.lon));
    }
}

void PlaneSpotter::drawPlane(Aircraft aircraft, boolean isSpecial) {
  Coordinates coordinates;
  coordinates.lon = aircraft.lon;
  coordinates.lat = aircraft.lat;  
  CoordinatesPixel p = geoMap_->convertToPixel(coordinates);
  tft_->setTextPadding(0);
  tft_->setTextColor(TFT_BLACK, TFT_LIGHTGREY);
  tft_->setTextDatum(BC_DATUM);
  tft_->drawString(aircraft.call, p.x + 8,p.y - 5, 1 );
  
  int planeDotsX[planeDots_];
  int planeDotsY[planeDots_];
  //isSpecial = false;
  for (int i = 0; i < planeDots_; i++) {
    planeDotsX[i] = cos((-450 + (planeDeg_[i] + aircraft.heading)) * PI / 180) * planeRadius_[i] + p.x; 
    planeDotsY[i] = sin((-450 + (planeDeg_[i] + aircraft.heading)) * PI / 180) * planeRadius_[i] + p.y; 
  }
  if (isSpecial) {
    tft_->fillTriangle(planeDotsX[0], planeDotsY[0], planeDotsX[1], planeDotsY[1], planeDotsX[2], planeDotsY[2], TFT_RED);
    tft_->fillTriangle(planeDotsX[2], planeDotsY[2], planeDotsX[3], planeDotsY[3], planeDotsX[4], planeDotsY[4], TFT_RED);
  } else {
      for (int i = 1; i < planeDots_; i++) {
        tft_->drawLine(planeDotsX[i], planeDotsY[i], planeDotsX[i-1], planeDotsY[i-1], TFT_RED);
      }
  }
}

String PlaneSpotter::drawInfoBox(Aircraft closestAircraft) {
  int line1 = geoMap_->getMapHeight() + 20;
  int line2 = geoMap_->getMapHeight() + 30;
  int line3 = geoMap_->getMapHeight() + 40;
  int right = tft_->width();
  //tft_->fillRect(0, geoMap_->getMapHeight(), tft_->width(), tft_->height() - geoMap_->getMapHeight(), TFT_BLACK);
  if (closestAircraft.call != "") {

    int xwidth = tft_->textWidth("ABC1234XY", GFXFONT ) + 12;
    tft_->setTextPadding(xwidth);
    tft_->setTextDatum(BL_DATUM);
    tft_->setTextColor(TFT_WHITE, TFT_BLACK);
    tft_->drawString(closestAircraft.call, 0, line1, GFXFONT );

    tft_->setTextPadding(480 - xwidth);
    tft_->setTextDatum(BR_DATUM);
    tft_->drawString(closestAircraft.aircraftType, right - 2, line1, GFXFONT );
    
    tft_->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft_->setTextDatum(BL_DATUM);
    xwidth = tft_->textWidth("Alt: XXXXXft", GFXFONT ) + 12;
    tft_->setTextPadding(xwidth);
    tft_->drawString("Alt: " + String(closestAircraft.altitude) + "ft", 0, line2, GFXFONT );
    
    int xpos = xwidth;
    tft_->setTextDatum(BL_DATUM);
    xwidth = tft_->textWidth("Spd : XXXXkn", GFXFONT ) + 10;
    tft_->setTextPadding(xwidth);
    tft_->drawString("Spd: " + String(closestAircraft.speed, 0) + "kn", xpos, line2, GFXFONT );

    xpos += xwidth;
    tft_->setTextDatum(BL_DATUM);
    xwidth = tft_->textWidth("Spd : XXXXkn", GFXFONT ) + 10;
    tft_->setTextPadding(xwidth);
    tft_->drawString("Dst: " + String(closestAircraft.distance, 2) + "km", xpos, line2, GFXFONT );

    tft_->setTextDatum(BL_DATUM);
    xwidth = tft_->textWidth("Hdg: 000", GFXFONT );
    tft_->setTextPadding(xwidth);
    tft_->drawString("Hdg: " + String(closestAircraft.heading, 0), right - xwidth, line2, GFXFONT );
  
    if (closestAircraft.fromShort != "" && closestAircraft.toShort != "") {
    return "From: " + closestAircraft.fromShort + "=>" + closestAircraft.toShort;
    }
  }
  return "";
}

void PlaneSpotter::jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

/* addded */
/* added */
/* addded */
/* added */


void PlaneSpotter::drawMainMenu() {
  tft_->setTextFont(2);
  String commands[] = {"Track", "Weather Station", "Planespotter"};
  int numberOfCommands = 3;
  int fontHeight = 24;
  for (int i = 0; i < numberOfCommands; i++) {
     int buttonHeight = 40;
    tft_->drawFastHLine(0, i * buttonHeight, tft_->width(), TFT_WHITE);
    drawString(10, i * buttonHeight + (buttonHeight - fontHeight) / 2, commands[i]); 

  }
}



void PlaneSpotter::PanAndZoom() {
  tft_->setTextFont(2);
        int fontHeight = 24;
        String Zoom_in = "Zoom in";
        String Zoom_out = "Zoom out";
        String UP = "Up";
        String Down = "Down"; 
        String Left = "Left";
        String Right = "Right";
     drawString(420, 230 - fontHeight, Zoom_in );
     drawString(420, 10 + fontHeight, Zoom_out );
     drawString(240, 186 + fontHeight, UP );
     drawString(240, 10  + fontHeight, Down );
     drawString(10, 96 - fontHeight, Left );
     drawString(470,96 - fontHeight, Right );

 
}

 
void PlaneSpotter::drawString(int x, int y, char *text) {
  int16_t x1, y1;
  uint16_t w, h;
  tft_->setTextWrap(false);

 tft_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  switch (alignment_) {
    case LEFT:
      x1 = x;
      break;
    case CENTER:
      x1 = x - w / 2;
      break;
    case RIGHT:
      x1 = x - w;
      break;
  }
  if (textColor_ != backgroundColor_) {
    tft_->fillRect(x1, y - h -1, w + 2, h + 3, backgroundColor_);
  }
  tft_->setCursor(x1, y);
  tft_->print(text);
}

void PlaneSpotter::drawString(int x, int y, String text) {
  char buf[text.length()+2];
  text.toCharArray(buf, text.length() + 1);
  drawString(x, y, buf);
}

void PlaneSpotter::setTextColor(uint16_t c) {
  setTextColor(c, c);
}
void PlaneSpotter::setTextColor(uint16_t c, uint16_t bg) {
  textColor_ = c;
  backgroundColor_ = bg;
  tft_->setTextColor(textColor_, backgroundColor_);
}

void PlaneSpotter::setTextAlignment(TextAlignment alignment) {
  alignment_ = alignment;
}
/* End */




void PlaneSpotter::setTouchScreen(XPT2046_Touchscreen* touchScreen) {
  touchScreen_ = touchScreen;
}


//calibrate not quite working, missing minX etc parameters
void PlaneSpotter::setTouchScreenCalibration(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY) {
//  minX_ = minX;
//  minY_ = minY;
//  maxX_ = maxX;
//  maxY_ = maxY;
}

CoordinatesPixel PlaneSpotter::getTouchPoint() {
    TS_Point pt = touchScreen_->getPoint();
    CoordinatesPixel p;
 //   p.x = tft_->width() * (pt.x - minX_) / (maxX_ - minX_);
     p.x = tft_->width() * (pt.x);
 //  p.y = tft_->height() * (pt.y - minY_) / (maxY_ - minY_);
    p.y = tft_->height()* (pt.y) ;
    return p;
}
