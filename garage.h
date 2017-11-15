#ifndef GARAGE_H
#define GARAGE_H

#include <Arduino.h>
#include <vector>
#include <functional>

typedef std::function<void()> Delegate;

class GarageClass
{
private:
  volatile bool isDoorOpen;

  Delegate onToggleCalback;
  Delegate onResetCalback;
  Delegate onRestartCalback;
  Delegate onOpenCalback;
  Delegate onCloseCalback;

public:
  GarageClass() : isDoorOpen(false)
  {
  }

  void onToggle(Delegate callback)
  {
    onToggleCalback = callback;
  }

  void onRestart(Delegate callback)
  {
    onRestartCalback = callback;
  }

  void onReset(Delegate callback)
  {
    onResetCalback = callback;
  }

  void onOpen(Delegate callback)
  {
    onOpenCalback = callback;
  }

  void onClose(Delegate callback)
  {
    onCloseCalback = callback;
  }

  String toJSON()
  {
    return "{\r\n \"open\":" + (String)isDoorOpen + "\r\n}";
  }

  void toggle()
  {
    if (onToggleCalback) onToggleCalback();
  }

  bool isOpen()
  {
    return isDoorOpen;
  }

  void open()
  {
    if (onOpenCalback)
    {
      onOpenCalback();
      isDoorOpen = true;
    }
  }

  void close()
  {
    if (onCloseCalback) 
    {
      onCloseCalback();
      isDoorOpen = false;
    }
  }

  void restart()
  {
    if (onRestartCalback) onRestartCalback();
  }

  void reset()
  {
    if (onResetCalback) onResetCalback();
  }
};

extern GarageClass Garage = GarageClass();

#endif
