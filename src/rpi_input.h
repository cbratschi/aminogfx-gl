#ifndef _AMINO_INPUT_RPI_H
#define _AMINO_INPUT_RPI_H

#include "base.h"

#include <linux/input.h>

//debug cbxx
#define DEBUG_INPUT true
#define DEBUG_INPUT_EVENTS false
#define DEBUG_TOUCH true

#define MAX_TOUCH_SLOTS 5

//helpers
#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

class AminoGfxRPi;
class AminoInputTouchSlot;

typedef struct {
    int x_min;
    int x_max;
    int y_min;
    int y_max;
} touch_grid_t;

/**
 * @brief Amino Input Handler.
 *
 */
class AminoInputRPi {
public:
    AminoInputRPi(AminoGfxRPi *amino, std::string filename);
    virtual ~AminoInputRPi();

    virtual bool init();
    virtual void process();

private:
    virtual bool initTouch();

    virtual void handleEvent(input_event ev);
    virtual void dumpEvent(struct input_event *event);

    virtual void handleRelEvent(input_event ev);
    virtual void handleAbsEvent(input_event ev);
    virtual void handleSynEvent(input_event ev);
    virtual void handleMscEvent(input_event ev);
    virtual void handleKeyEvent(input_event ev);

    bool hasValidTouchSlot();

    virtual void fireTouchEvent();

    //amino
    AminoGfxRPi *amino;

    //device data
    std::string filename;
    int fd = -1;
    std::string name;
    std::string phys;

    //mouse (TODO not implemented)
    int mouseX = 0;
    int mouseY = 0;

    //touch
    touch_grid_t touchGrid;
    std::vector<AminoInputTouchSlot *> touchSlots;
    int currentTouchSlot = 0;
    bool touchStarted = false;
    bool touchModified = false;
};

/**
 * @brief Amino Input Touch Slot Handler.
 *
 */
class AminoInputTouchSlot {
public:
    AminoInputTouchSlot(int slot);

    int slot;
    int id = -1;
    int x = 0;
    int y = 0;
    int timestamp = 0;
    bool ready = false;
};

#endif