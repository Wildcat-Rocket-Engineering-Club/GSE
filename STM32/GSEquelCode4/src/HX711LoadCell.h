#ifndef HX711_LOAD_CELL_H
#define HX711_LOAD_CELL_H

#include <HX711.h>
#include <Print.h>

class HX711LoadCell {
public:
  HX711LoadCell();

  bool begin(Print &diagnostics);
  void update();
  float valueLbs() const;

private:
  HX711 scale;
  float loadLbs;
  bool ready;
};

#endif
