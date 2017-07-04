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

#include "PlaneSpotter.h"

uint16_t read16(File & f);
uint32_t read32(File & f);


PlaneSpotter::PlaneSpotter(TFT_eSPI* tft, GeoMap* geoMap ) {
  tft_ = tft;
  geoMap_ = geoMap;
}


void PlaneSpotter::copyProgmemToSpiffs(const uint8_t *data, unsigned int length, String filename) {
  File f = SPIFFS.open(filename, "w+");
  uint8_t c;
  for(int i = 0; i < length; i++) {
    c = pgm_read_byte(data + i);
    f.write(c);
  }
  f.close();
}

// This drawBMP function contains code from:
// https://github.com/adafruit/Adafruit_ILI9341/blob/master/examples/spitftbitmap/spitftbitmap.ino
// Here is Bodmer's version: this uses the ILI9341 CGRAM coordinate rotation features inside the display and
// buffers both file and TFT pixel blocks, it typically runs about 2x faster for bottom up encoded BMP images

void PlaneSpotter::drawBmp(String filename, uint8_t x, uint16_t y) {
  // Flips the TFT internal SGRAM coords to draw bottom up BMP images faster, in this application it can be fixed
  boolean flip = 1;

  if ((x >= tft_->width()) || (y >= tft_->height())) return;

  fs::File bmpFile;
  int16_t  bmpWidth, bmpHeight;   // Image W+H in pixels
  uint32_t bmpImageoffset;        // Start address of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL];    // file read pixel buffer (8 bits each R+G+B per pixel)
  uint16_t tftbuffer[BUFFPIXEL];       // TFT pixel out buffer (16-bit per pixel)
  uint8_t  rgb_ptr = sizeof(sdbuffer); // read 24 bit RGB pixel data buffer pointer (8 bit so BUFF_SIZE must be less than 86)
  boolean  goodBmp = false;            // Flag set to true on valid header parse
  int16_t  w, h, row, col;             // to store width, height, row and column
  uint8_t rotation;      // to restore rotation
  uint8_t  tft_ptr = 0;  // TFT 16 bit 565 format pixel data buffer pointer

  // Check file exists and open it
  Serial.println(filename);
  if ( !(bmpFile = SPIFFS.open(filename, "r")) ) {
    Serial.println(F(" File not found")); // Can comment out if not needed
    return;
  }

  // Parse BMP header to get the information we need
  if (read16(bmpFile) == 0x4D42) { // BMP file start signature check
    read32(bmpFile);       // Dummy read to throw away and move on
    read32(bmpFile);       // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    read32(bmpFile);       // Dummy read to throw away and move on
    bmpWidth  = read32(bmpFile);  // Image width
    bmpHeight = read32(bmpFile);  // Image height

    // Only proceed if we pass a bitmap file check
    // Number of image planes -- must be '1', depth 24 and 0 (uncompressed format)
    if ((read16(bmpFile) == 1) && (read16(bmpFile) == 24) && (read32(bmpFile) == 0)) {
      goodBmp = true; // Supported BMP format
      // BMP rows are padded (if needed) to 4-byte boundary
      rowSize = (bmpWidth * 3 + 3) & ~3;
      // Crop area to be loaded
      w = bmpWidth;
      h = bmpHeight;

      // We might need to alter rotation to avoid tedious file pointer manipulation
      // Save the current value so we can restore it later
      rotation = tft_->getRotation();
      // Use TFT SGRAM coord rotation if flip is set for 25% faster rendering (new rotations 4-7 supported by library)
      if (flip) tft_->setRotation((rotation + (flip<<2)) % 8); // Value 0-3 mapped to 4-7

      // Calculate new y plot coordinate if we are flipping
      switch (rotation) {
        case 0:
          if (flip) y = tft_->height() - y - h; break;
        case 1:
          y = tft_->height() - y - h; break;
          break;
        case 2:
          if (flip) y = tft_->height() - y - h; break;
          break;
        case 3:
          y = tft_->height() - y - h; break;
          break;
      }

      // Set TFT address window to image bounds
      // Currently, image will not draw or will be corrputed if it does not fit
      // TODO -> efficient clipping, but I don't need it to be idiot proof ;-)
      tft_->setAddrWindow(x, y, x + w - 1, y + h - 1);

      // Finally we are ready to send rows of pixels, writing like this avoids slow 32 bit multiply in 8 bit processors
      for (uint32_t pos = bmpImageoffset; pos < bmpImageoffset + h * rowSize ; pos += rowSize) {
        // Seek if we need to on boundaries and arrange to dump buffer and start again
        if (bmpFile.position() != pos) {
          bmpFile.seek(pos, fs::SeekSet);
          rgb_ptr = sizeof(sdbuffer);
          //Serial.println("Seeking in file >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
        }

        // Fill the pixel buffer and plot
        for (col = 0; col < w; col++) { // For each column...
          // Time to read more pixel data?
          if (rgb_ptr >= sizeof(sdbuffer)) {
            // Push tft buffer to the display
            if (tft_ptr) {
              // Here we are sending a uint16_t array to the function
              tft_->pushColors(tftbuffer, tft_ptr);
              tft_ptr = 0; // tft_ptr and rgb_ptr are not always in sync...
            }
            // Finally reading bytes from SD Card
            bmpFile.read(sdbuffer, sizeof(sdbuffer));
            rgb_ptr = 0; // Set buffer index to start
          }
          // Convert pixel from BMP 8+8+8 format to TFT compatible 16 bit word
          // Blue 5 bits, green 6 bits and red 5 bits (16 bits total)
          // Is is a long line but it is faster than calling a library fn for this
          tftbuffer[tft_ptr] = (sdbuffer[rgb_ptr++] >> 3) ;
          tftbuffer[tft_ptr] |= ((sdbuffer[rgb_ptr++] & 0xFC) << 3);
          tftbuffer[tft_ptr] |= ((sdbuffer[rgb_ptr++] & 0xF8) << 8);
          tft_ptr++;
        } // Next row
      }   // All rows done

      // Write any partially full buffer to TFT
      if (tft_ptr) tft_->pushColors(tftbuffer, tft_ptr);

    } // End of bitmap access
  }   // End of bitmap file check

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognised."));
  tft_->setRotation(rotation); // Put back original rotation
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}


void PlaneSpotter::drawSPIFFSJpeg(String filename, int xpos, int ypos) {
  
  Serial.println(filename);
  char buffer[filename.length() + 1];
  filename.toCharArray(buffer, filename.length() + 1);
  JpegDec.decodeFile(buffer);
  renderJPEG(xpos, ypos);
  
}

void PlaneSpotter::renderJPEG(int xpos, int ypos) {

  uint16_t  *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t mcu_pixels = mcu_w * mcu_h;
  uint32_t drawTime = millis();

  while( JpegDec.read()){
    
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;
    if ( ( mcu_x + mcu_w) <= tft_->getWidth() && ( mcu_y + mcu_h) <= tft_->getHeight()){
      
      tft_->setAddrWindow(mcu_x, mcu_y, mcu_x + mcu_w - 1, mcu_y + mcu_h - 1);
      uint32_t count = mcu_pixels;
      while (count--) {tft_->pushColor(*pImg++);}
      // Push all MCU pixels to the TFT window, ~18% faster to pass an array pointer and length to the library
      //tft_->pushColor16(pImg, mcu_pixels); //  To be supported in HX8357 library at a future date

    }

    else if( ( mcu_y + mcu_h) >= tft_->getHeight()) JpegDec.abort();
  
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

// This draws callsign box
void PlaneSpotter::drawPlane(Aircraft aircraft, boolean isSpecial) {
  Coordinates coordinates;
  coordinates.lon = aircraft.lon;
  coordinates.lat = aircraft.lat;  
  CoordinatesPixel p = geoMap_->convertToPixel(coordinates);
  
  setTextColor(TFT_WHITE, TFT_BLACK);
  setTextAlignment(CENTER);
  //drawString(p.x + 8,p.y - 5, aircraft.call);
  drawString(p.x, p.y, aircraft.call);
  
  // This draws the red Triangle
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

// Todo 
//   int species;
//   species = geomap.getSpecies;   
//  if (species =


void PlaneSpotter::drawInfoBox(Aircraft closestAircraft) {
  int line1 = geoMap_->getMapHeight() + 7;
  int line2 = geoMap_->getMapHeight() + 20;
  int line3 = geoMap_->getMapHeight() + 31;
  int line4 = geoMap_->getMapHeight() + 41;
  int line5 = geoMap_->getMapHeight() + 52;
  int linetype = geoMap_->getMapHeight() + 24;
  //  int rightTab0 = tft_->getWidth() - 3; //
  int leftTab1 = 8;
  int leftTabBA = 3; 
  int leftTab2 = 68; // was 40
  int leftTabBB = tft_->getWidth()/ 2 + 86 ; 
  int leftTab3 = tft_->getWidth() / 2 + 91 ; // was / 2
  int leftTab4 = leftTab3 + 60; // was leftfTab3 + 40
  int leftTabtype = tft_->getWidth() /2 -42;  
  //  int towerMenuX = geoMap_->getMapHeight()+312;
  //  int towerMenuY = geoMap_->getMapWidth () +310 ;     
  //tft_->setFont(&Dialog_plain_9);
  int fontHeight = 15;
/*Heavily modified  by Ierlandfan for bigger screen and to suit my needs */ 
/*  Added heading indicator, added silhouettes (by downloading them for my server based on ICAO designator) changed text positions and added vspeed and registration */

/* Tower image is for to-do menu) */
//drawSPIFFSJpeg("/tower.jpg",towerMenuX,towerMenuY);     
  
  if (closestAircraft.call != "") {          
 //Background InfoBox
  tft_->fillRect(0, geoMap_->getMapHeight(), tft_->getWidth(), tft_->getHeight() - geoMap_->getMapHeight(),TFT_BLUE_SKY);
  //left InfoBox  
  tft_->fillRoundRect(leftTabBA, line1,  151 , 42, 4, TFT_BLACK);
  //Right InfoBox
  tft_->fillRoundRect(leftTabBB, line1,  151 , 42, 4, TFT_BLACK);  
//Silhouette 
  tft_->drawRoundRect(156, line1,  168 , 42, 4, TFT_BLACK);    

// Species/TypeOfAircraft 
    setTextAlignment(LEFT);
    setTextColor(TFT_GREEN);
    drawString(leftTab1, line1, "Type: ");

    setTextAlignment(LEFT);
    //tft_->drawRoundRect(leftTab3, line1, tft_->getWidth() - 3 , fontHeight, 4, TFT_BLACK);
    setTextColor(TFT_GREEN);
    drawString(leftTab3, line1, "Reg: ");
    setTextColor(TFT_WHITE);
    drawString(leftTab4, line1, closestAircraft.registration);

    tft_->fillRoundRect(156, line1,  168 , 16, 3, TFT_BLACK);
    //setTextColor(TFT_GREEN);
    //drawString(159, line1, "Brand: "); //Changed to Brand because of type declared later for type of airplane
    setTextAlignment(LEFT);
    setTextColor(TFT_WHITE);
    drawString(159, line1, closestAircraft.aircraftType); //was 230
    Serial.println("Aircraft-Model: " + closestAircraft.aircraftType);
    
    //tft_->drawRoundRect(leftTab1, line2, leftTab1 + leftTab2, fontHeight, 4, TFT_BLACK);
    setTextAlignment(LEFT);
    setTextColor(TFT_GREEN);
    drawString(leftTab1, line2, "Altitude: ");
    setTextColor(TFT_WHITE);
    drawString(leftTab2, line2, String(closestAircraft.altitude) + " ft");

    //tft_->drawRoundRect(leftTab3, line2, leftTab3 + leftTab4, fontHeight, 4, TFT_BLACK);
    setTextColor(TFT_GREEN);
    drawString(leftTab3, line2, "Heading: ");
    setTextColor(TFT_WHITE);
    drawString(leftTab4, line2, String(closestAircraft.heading, 0));

//Heading as a compass drawn in infobox

int degrees[] = {0, 170, 190, 0};
tft_->drawCircle(leftTab4 +60, geoMap_->getMapHeight() + 29, 8, TFT_WHITE); // x,y is the spot where it will be drawn
int radius = 6;  
for (int i = 0; i < 6; i++) {
int x1 = cos((-450 + (closestAircraft.heading + degrees[i])) * PI / 180) * planeRadius_[i] + leftTab4 +60;  
int y1 = sin((-450 + (closestAircraft.heading + degrees[i])) * PI / 180) * planeRadius_[i] + geoMap_->getMapHeight() + 29; //was 29
int x2 = cos((-450 + (closestAircraft.heading + degrees[i+1])) * PI / 180) * planeRadius_[i] + leftTab4 +60; 
int y2 = sin((-450 + (closestAircraft.heading + degrees[i+1])) * PI / 180) * planeRadius_[i] + geoMap_->getMapHeight() + 29; 
     tft_->drawLine(x1, y1, x2, y2, TFT_RED);
}
  //End Heading as compass draw


    
    //tft_->drawRoundRect(leftTab1, line3, leftTab1 + leftTab2, fontHeight, 4, TFT_BLACK);
    setTextColor(TFT_GREEN);
    drawString(leftTab1, line3, "Distance: ");
    setTextColor(TFT_WHITE);
    drawString(leftTab2, line3, String(closestAircraft.distance, 2) + " km");
    
    
    //tft_->drawRoundRect(leftTab3, line3, leftTab3 + leftTab4, fontHeight, 4, TFT_BLACK);
    setTextColor(TFT_GREEN);
    drawString(leftTab3, line3, "HSpeed: ");
    setTextColor(TFT_WHITE);
    drawString(leftTab4, line3, String(closestAircraft.speed, 0) + " kn");
 
//do something with the texbounds so that it displays the complete name based on length so that X position is based on length,
    //tft_->fillRoundRect(397, 3, 80, fontHeight, 2, TFT_BLACK);
    setTextColor(TFT_WHITE, TFT_BLACK);
    drawString(350, 3, String(closestAircraft.airlineOperator ));



//if (closestAircraft.Sqwak.toInt() == 7500 || 7700 )  
//{
//    tft_->fillRoundRect(190, 3, 80, 17, 2, TFT_BLACK);
//    setTextColor(TFT_RED);
//    drawString(193, 3, "EMERGENCY");

//   tft_->fillRoundRect(3, 3, 90, 17, 2, TFT_BLACK);
//    setTextColor(TFT_RED);
//    drawString(6, 3, "Sqwak: ");
 //   setTextColor(TFT_RED);
 //   drawString(56, 3, String(closestAircraft.Sqwak));
//}   
  //  else if (closestAircraft.Sqwak.toInt() == 7000 ) 
//{
   // tft_->fillRoundRect(3, 3, 90, fontHeight, 2, TFT_BLACK);
   // setTextColor(TFT_GREEN);
   // drawString(6, 3, "Sqwak: ");
   // setTextColor(TFT_GREEN);
   // drawString(56, 3, String(closestAircraft.Sqwak));
 
  
  //}

 //else if (closestAircraft.Sqwak != "") 
//{
    tft_->fillRoundRect(3, 3, 90, fontHeight, 2, TFT_BLACK);
    setTextColor(TFT_GREEN);
    drawString(6, 3, "Sqwak: ");
    setTextColor(TFT_WHITE);
    drawString(56, 3, String(closestAircraft.Sqwak));
//}
  
  
///Type in text format -- custom for The Netherlands

  int Species = closestAircraft.species;
const char * tag = closestAircraft.registration.c_str ();
   String chassis;      
uint16_t chassiscolor = TFT_WHITE;
String enginetype = closestAircraft.enginetype;   
   switch(Species) {
      case 1: {
Serial.println("TAG is:");
Serial.println(tag);
int i =0;
for (i=0;i<strlen(tag); i++){

if ( (tag[i] == 'P') && (tag[i] == 'H')&& (tag[i] == '-') && ((tag[i] >= '0' && tag[i] <= '9')) && ((tag[i] >= '0' && tag[i] <= '9'))&& (( tag[i] >= '0' && tag[i] <= '9')) && ((tag[i] >= '0' && tag[i] <= '9')))  
// Glider
{
chassis = "Glider";
}

else if ( (tag[i] == 'P') && (tag[i] == 'H') &&  (tag[i] == '-') &&  ((tag[i] >= '0' && tag[i] <= '9')) &&  ((tag[i] >= 'A' && tag[i] <= 'Z')) && ((tag[i] >= 'A' && tag[i] <= 'Z')) )  
// Drone
{
chassis = "Drone";
}
else if 
( (tag[i] == 'P')&& (tag[i] == 'H') && (tag[i] == '-') &&  ((tag[i] >= '0' && tag[i] <= '9')) && ((tag[i] >= 'A' && tag[i] <= 'Z')) && ((tag[i] >= '0' && tag[i] <= '9')) )
//Ultralight
{
chassis = "Ultralight";
//draw jpeg ultralight
chassiscolor = TFT_LIGHTGREY;
}
}     
if(enginetype == "1")
{
//Draw GA like plane 
chassiscolor = TFT_RED;
chassis = "GA";
}

else if (enginetype == "2")

{
chassis = "Turboprop";
}
else if (enginetype == "3")
{
chassis = "Jet";
}
 
else if (enginetype == "4")
{
chassis = "Electric";
}
else
{
chassis = "Land Plane";
}
   } //case 1
      
       break;
      case 2: {
       chassis = "Sea plane"; 
      chassiscolor = TFT_BLUE_SKY;
      }
         break;
      case 3: {
         chassiscolor = TFT_GREEN;
         chassis = "Amphibian";
      }
         break;
      case 4: {
        chassiscolor = TFT_NAVY;
        chassis = "Helicopter";
      }
         break;
      case 5: {
        chassis = "Gyrocopter";
      }
         break;
      case 6: {
        chassis = "Tiltwing";
      }
         break;
      case 7: {
        chassis = "Ground Vehicle";
      }
         break;
      case 8: {
         chassis = "Tower";
      }
      break;       
     default: {
        chassis = "N/A";
     }
   }
    setTextColor(chassiscolor);
    drawString(leftTab2, line1, chassis);



if (closestAircraft.fromShort != "" && closestAircraft.toShort != "") {
      tft_->fillRoundRect(leftTabBA, line5, tft_->getWidth() - 4 , 15, 2, TFT_BLACK);
      setTextColor(TFT_GREEN);
      
      drawString(185, 303, ": From ");
      setTextColor(TFT_WHITE);
      setTextAlignment(LEFT);
      drawString(leftTab1, 303, (String ( (closestAircraft.fromCode) + (closestAircraft.fromShort) ) ) ); // Changed, now under eacht other and made shorter to fit screen better
       setTextAlignment(LEFT);
      setTextColor(TFT_GREEN);
        setTextAlignment(LEFT);
      drawString(230, 303, "To : ");    // Changed
            setTextColor(TFT_WHITE);
            drawString(280, 303, (String( (closestAircraft.toCode) + (closestAircraft.toShort) ) ) ); // Changed
  
       }
 
  }
}
 
void PlaneSpotter::drawSilhouettes(Aircraft  closestAircraft) {

  // Display the silhouettes //
  int linetype = geoMap_->getMapHeight() + 24;
  int leftTabtype = tft_->getWidth() /2 -42;
    String silhouettettype = "A319.bmp";
    String notimportant = "";  
    String url = "http://192.168.1.100:8080:/silhouettes/A319.bmp";
  // geoMap_->downloadsilhouette(url, silhouettettype);  //Hashed out for now, it crashes the ESP fr some unknown reason
  delay(2000);
  yield();
   drawBmp(silhouettettype, leftTabtype, linetype);      
}

     
void PlaneSpotter::drawMainMenu() {
  tft_->setTextFont(2);
  String commands[] = {"Presets", "Track", "Weather Station", "Planespotter", "Back"};
  String menutitle = {"Main Menu"};
  tft_->fillScreen(TFT_BLACK);
  int numberOfCommands = 5;
  int fontHeight = 24;
  int buttonHeight = 40;
  drawString(10, buttonHeight - fontHeight / 2, menutitle);  //Is this even working?
     for (int i = 0; i < numberOfCommands; i++) {
      tft_->drawFastHLine(0, i * buttonHeight, tft_->getWidth(), TFT_WHITE);
    drawString(20, i * buttonHeight + (buttonHeight - fontHeight) / 2, commands[i]); 

  }
}




// Todo -- Define defaults somewehere in Settings
//Jump to predefined menu's //

String Preset1 = "EHAM, Amsterdam Schiphol";
String Preset2 = "EHLE, Lelystad Airport"; 
String Preset3 = "EHEH, Eindhoven Airport";
String Preset4 = "EHRD, Rotterdam Airport";
String Preset5 = "EHKD, Den Helder";
String Preset6 = "EHTX, Texel Airport";
String Preset7 = "Current location";
String Preset8 = "Back";
//to do -- create menu to jump to predefined menu's 
void PlaneSpotter::drawPresetMenu() {
  tft_->setTextFont(2);
  String commands[] = {Preset1, Preset2, Preset3, Preset4, Preset5, Preset6, Preset7, Preset8};
  String menutitle = {"Preset Menu"};
  tft_->fillScreen(TFT_BLACK);
  int numberOfCommands = 7;
  int fontHeight = 24;
 int buttonHeight = 40;
  drawString(10, buttonHeight - fontHeight / 2, menutitle);  //Is this even working?
  for (int i = 0; i < numberOfCommands; i++) {
    tft_->drawFastHLine(0, i * buttonHeight, tft_->getWidth(), TFT_WHITE);
    drawString(20, i * buttonHeight + (buttonHeight - fontHeight) / 2, commands[i]); 
     

  }
}




void PlaneSpotter::drawZoomAndPanMenu() {
  tft_->setTextFont(2);
        int fontHeight = 24;
        String Zoom_in = "Zoom in";
        String Zoom_out = "Zoom out";
        String Reset    = "Reset";
        String UP = "Up";
        String Down = "Down"; 
        String Left = "Left";
        String Right = "Right";

//int PHline1 = geoMap_->getMapHeight() /2;
//int PVline1 = geoMap_->getMapWidth()  /2;
//int ZHline1 = geoMap_->getMapWidth()  /4; 
//int ZVline1 = geoMap_->getMapHeight() /4;  
     drawString(420, 40 - fontHeight, Zoom_in );
     drawString(420, 240 - fontHeight, Zoom_out );
     drawString(220, 125 - fontHeight, Reset );
     drawString(220, 20 - fontHeight, UP );
     drawString(220, 250 - fontHeight, Down );
     drawString(4, 125 - fontHeight, Left );
     drawString(460, 125 - fontHeight, Right );
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

void PlaneSpotter::setTouchScreen(XPT2046_Touchscreen* touchScreen) {
  touchScreen_ = touchScreen;
}

void PlaneSpotter::setTouchScreenCalibration(uint16_t minX, uint16_t minY, uint16_t maxX, uint16_t maxY) {
  minX_ = minX;
  minY_ = minY;
  maxX_ = maxX;
  maxY_ = maxY;
}

CoordinatesPixel PlaneSpotter::getTouchPoint() {
    TS_Point pt = touchScreen_->getPoint();
    CoordinatesPixel p;
    p.y = tft_->getHeight() * (pt.y - minY_) / (maxY_ - minY_);
    p.x = tft_->getWidth() * (pt.x - minX_) / (maxX_ - minX_);
    return p;
}

