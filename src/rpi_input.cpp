#include "rpi_input.h"

//
// AminoInputRPi
//

AminoInputRPi::AminoInputRPi(AminoGfxRPi *amino, std::string filename) : amino(amino), filename(filename) {
    //no code
}

AminoInputRPi::~AminoInputRPi() {
    close(fd);
}

bool AminoInputRPi::init() {
    // 1) open device
    if ((fd = open(filename, O_RDONLY | O_NONBLOCK)) == -1) {
        printf("This is not a valid device %s\n", filename);
        return false;
    }

    // 2) get device name
    char name[256] = "Unknown";

    ioctl(fd, EVIOCGNAME(sizeof name), name);
    this->name = name;

    printf("Reading from: %s (%s)\n", filename, name);

    // 3) get physical name
    char phys[256] = "Unknown";

    ioctl(fd, EVIOCGPHYS(sizeof phys), phys);
    this->phys = phys;

    printf("Location %s (%s)\n", filename, phys);

    //get device info (see https://elixir.bootlin.com/linux/v4.2/source/include/uapi/linux/input.h#L41)
    /*
    struct input_id device_info;

    ioctl(fd, EVIOCGID, &device_info);
    */

   // 4) check event types

    //iterate event types
    u_int8_t evtype_b[(EV_MAX + 7) / 8];

    memset(evtype_b, 0, sizeof evtype_b);

    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
        printf("Error reading device info.\n");
        return false;
    }

    for (int i = 0; i < EV_MAX; i++) {
        if (test_bit(i, evtype_b)) {
            //see https://www.kernel.org/doc/Documentation/input/event-codes.txt
            printf("Event type 0x%02x ", i);

            switch (i) {
                case EV_SYN:
                    printf("sync events\n");
                    break;

                case EV_KEY:
                    //keyboard, buttons, ...
                    printf("key events\n");
                    break;

                case EV_REL:
                    printf("rel events\n");
                    break;

                case EV_ABS:
                    //touchscreen
                    printf("abs events\n");
                    break;

                case EV_MSC:
                    //other input data
                    printf("msc events\n");
                    break;

                case EV_SW:
                    printf("sw events\n");
                    break;

                case EV_LED:
                    printf("led events\n");
                    break;

                case EV_SND:
                    printf("snd events\n");
                    break;

                case EV_REP:
                    printf("rep events\n");
                    break;

                case EV_FF:
                    printf("ff events\n");
                    break;

                default:
                    printf("unknown\n");
                    break;
            }
        }
    }

    //init touch devices
    if (test_bit(EV_ABS, evtype_b)) {
        if (!initTouch(fd)) {
            return false;
        }
    }

    return true;
}

bool AminoInputRPi::initTouch() {
    //get EV_ABS keys
    u_int8_t abskey_b[(KEY_MAX + 7) / 8];

    memset(abskey_b, 0, sizeof abskey_b);

    if (ioctl(fd, EVIOCGBIT(EV_ABS, KEY_MAX), abskey_b) < 0) {
        printf("Error reading touch info.\n");
        return;
    }

    //get values
    struct input_absinfo abs_feat;

    // 1) get x grid
    assert(test_bit(ABS_X, abskey_b));

    memset(abs_feat, 0, sizeof abs_feat);
    assert(ioctl(fd, EVIOCGABS(ABS_X), abs_feat) >= 0);

    touchGrid.x_min = abs_feat.minimum;
    touchGrid.x_max = abs_feat.maximum;

    // 2) get y grid
    assert(test_bit(ABS_Y, abskey_b));

    memset(abs_feat, 0, sizeof abs_feat);
    assert(ioctl(fd, EVIOCGABS(ABS_Y), abs_feat) >= 0);

    touchGrid.y_min = abs_feat.minimum;
    touchGrid.y_max = abs_feat.maximum;

    // 3) init buffers
    for (int i = 0; i < MAX_TOUCH_SLOTS; i++) {
        touchSlots.push_back(new AminoInputTouchSlot(i))
    }

    if (DEBUG_TOUCH) {
        printf("Touch grid horz: %d ... %d\n", touchGrid.x_min, touchGrid.x_max);
        printf("Touch grid vert: %d ... %d\n", touchGrid.y_min, touchGrid.y_max);
    }

    return true;
}

AminoInputRPi::process() {
    //read events
    int size = sizeof struct input_event;
    struct input_event ev[64];

    int rd = read(fd, ev, size * 64);

    if (rd == -1) {
        continue;
    }

    int items = rd / size;

    if (items == 0) {
        printf("Read too little!!!  %d\n", rd);
        return;
    }

    //debug
    //printf("Read %d bytes from %d.\n", rd, fd);

    for (int i = 0; i < items; i++) {
        if (DEBUG_INPUT_EVENTS) {
            dumpEvent(&(ev[i]));
        }

        handleEvent(ev[i]);
    }
}

void AminoInputRPi::handleEvent(input_event ev) {
    switch (ev.code) {
        case EV_REL:
            //mouse events
            handleRelEvent(ev);
            break;

        case EV_ABS:
            //touch events
            handleAbsEvent(ev);
            break;

        case EV_SYN:
            //synchronization (touch)
            handleSynEvent(ev);
            break;

        case EV_MSC:
            //misc
            handleMscEvent(ev);
            break;

        case EV_KEY:
            //keyboard (button, touch state)
            handleKeyEvent(ev);
            break;
    }
}

void AminoInputRPi::handleRelEvent(input_event ev) {
    //relative event: probably mouse
    switch (ev.code) {
        case REL_X:
            //x axis
            mouseX += ev.value;

            if (mouseX < 0) {
                mouseX = 0;
            }

            if (mouseX >= (int)amino->screenW)  {
                mouseX = (int)amino->screenW - 1;
            }

            break;

        case REL_Y:
            //y axis
            mouseY += ev.value;

            if (mouseY < 0) {
                mouseY = 0;
            }

            if (mouseY >= (int)amino->screenH) {
                mouseY = (int)amino->screenH - 1;
            }

            break;

        case REL_WHEEL:
            //mouse wheel
            //TODO GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(ev.value);
            break;
    }

    //TODO GLFW_MOUSE_POS_CALLBACK_FUNCTION(mouse_x, mouse_y);
    //TODO send mouse events
}

void AminoInputRPi::handleAbsEvent(input_event ev) {
    //see https://elixir.bootlin.com/linux/v4.6/source/include/uapi/linux/input-event-codes.h#L682
    //Note: only supporting protocol B touch devices (otherwise mtdev has to be used to convert the events)
    switch (ev.code) {
        case ABS_X: //0
            //slot 0 fallback
        case ABS_MT_POSITION_X: //53
            //x value
            if (ev.value > 0) {
                if (DEBUG_TOUCH) {
                    printf("-> touch x%d: %d\n", currentTouchSlot, ev.value);
                }

                if (hasValidTouchSlot()) {
                    touchSlots[currentTouchSlot].x = ev.value;
                    touchSlots[currentTouchSlot].ready = false;
                    touchModified = true;
                }
            }
            break;

        case ABS_Y: //1
            //slot 0 fallback
        case ABS_MT_POSITION_Y: //54
            //y value
            if (ev.value > 0) {
                if (DEBUG_TOUCH) {
                    printf("-> touch y%d: %d\n", currentTouchSlot, ev.value);
                }

                if (hasValidTouchSlot()) {
                    touchSlots[currentTouchSlot].y = ev.value;
                    touchSlots[currentTouchSlot].ready = false;
                    touchModified = true;
                }
            }
            break;

        case ABS_PRESSURE: //24
            //pressure value
            break;

        case ABS_MT_SLOT:
            if (ev.value == -1) {
                //end current slot
                if (hasValidTouchSlot()) {
                    //mark as invalid
                    touchSlots[currentTouchSlot].id = -1;
                    touchSlots[currentTouchSlot].ready = false;
                    touchModified = true;
                }

                //Note: resetting to zero (not -1) to avoid issues
                currentTouchSlot = 0;
            } else {
                //previous slot done
                if (hasValidTouchSlot()) {
                    touchSlots[currentTouchSlot].ready = true;

                    //Note: does not change modified state
                }

                //switch to different slot
                currentTouchSlot = ev.value;
            }
            break;

        case ABS_MT_TRACKING_ID: //57
            if (DEBUG_TOUCH) {
                printf("-> multitouch ID: %d\n", ev.value);
            }

            if (hasValidTouchSlot()) {
                touchSlots[currentTouchSlot].id = ev.value;
                touchModified = true;
            }
            break;

        default:
            if (DEBUG_TOUCH) {
                printf("-> unknown: %d % d\n", ev.code, ev.value);
            }
            break;
    }
}

bool AminoInputRPi::hasValidTouchSlot() {
    return currentTouchSlot >= 0 && currentTouchSlot < MAX_TOUCH_SLOTS;
}

void AminoInputRPi::handleSynEvent(input_event ev) {
    if (ev.code === SYN_REPORT) {
        if (DEBUG_TOUCH) {
            printf("-> report event\n");
        }

        //end of touch event
        fireTouchEvent();
    }
}

void AminoInputRPi::fireTouchEvent() {
    if (!touchModified) {
        return;
    }

    //create object
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("touch").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("pressed").ToLocalChecked(), Nan::New<v8::Boolean>(touchPressed));

    //count touchpoints
    int touchPoints = 0;
    v8::Local<v8::Array> arr = Nan::New<v8::Array>();

    if (touchStarted) {
        for (int i = 0; i < MAX_TOUCH_SLOTS; i++) {
            AminoInputTouchSlot *slot = touchSlots[i];

            if (slot->id && slot->ready) {
                //data
                v8::Local<v8::Object> touchpoint_obj = Nan::New<v8::Object>();
                int x = slot->x * amino->windowW / (touchGrid.x_max - touchGrid.x_min);
                int y = slot->y * amino->windowW / (touchGrid.y_max - touchGrid.y_min);

                Nan::Set(touchpoint_obj, Nan::New("id").ToLocalChecked(), Nan::New(slot->id));
                Nan::Set(touchpoint_obj, Nan::New("x").ToLocalChecked(), Nan::New(x));
                Nan::Set(touchpoint_obj, Nan::New("y").ToLocalChecked(), Nan::New(y));
                Nan::Set(touchpoint_obj, Nan::New("timestamp").ToLocalChecked(), Nan::New(slot->timestamp));

                Nan::Set(arr, Nan::New<v8::Uint32>(touchPoints), touchpoint_obj);

                touchPoints++;

                if (DEBUG_TOUCH) {
                    printf("=> touch event: x=%d y=%d id=%d timestamp=%d\n", x, y, slot->id, slot->timestamp);
                }
            }
        }
    }

    Nan::Set(event_obj, Nan::New("points").ToLocalChecked(), arr);
    Nan::Set(event_obj, Nan::New("count").ToLocalChecked(), Nan::New(touchPoints));

    //cbxx TODO only sent once per input cycle

    amino->fireEvent(event_obj);
}

void AminoInputRPi::handleMscEvent(input_event ev) {
    if (ev.code == MSC_TIMESTAMP) {
        if (hasValidTouchSlot()) {
                touchSlots[currentTouchSlot].timestamp = ev.value;
            }
        }
    }
}

void AminoInputRPi::handleKeyEvent(input_event ev) {
    if (DEBUG_INPUT) {
        printf("Key or button pressed code = %d, state = %d\n", ev.code, ev.value);
    }

    //touch events
    if (ev.code == BTN_TOUCH) { //330
        if (DEBUG_TOUCH) {
            printf("-> touch %s\n", touch_start ? "start":"finish");
        }

        //check start
        if (ev.value == 1) {
            touchStarted = true;

            //Note: not yet marked as modified
            return;
        }

        //handle finish
        if (touchStarted) {
            //reset all slots
            for (int i = 0; i < MAX_TOUCH_SLOTS; i++) {
                AminoInputTouchSlot *slot = touchSlots[i];

                slot->id = -1;
                slot->ready = false;
            }

            touchModified = true;
            touchStarted = false;
        }

        return;
    }

    //normal keys
    if (ev.code == BTN_LEFT) {
        //TODO GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(ev.code, ev.value);
        return;
    } else {
        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        if (!ev.value) {
            //release
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.release").ToLocalChecked());
        } else if (ev.value) {
            //press or repeat
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.press").ToLocalChecked());
        }

        //key codes
        int keycode = -1;

        if (ev.code >= KEY_1 && ev.code <= KEY_9) {
            keycode = ev.code - KEY_1 + 49;
        }

        switch (ev.code) {
            // from https://www.glfw.org/docs/latest/group__keys.html

            case KEY_0: keycode = 48; break;
            case KEY_1: keycode = 49; break;
            case KEY_2: keycode = 50; break;
            case KEY_3: keycode = 51; break;
            case KEY_4: keycode = 52; break;
            case KEY_5: keycode = 53; break;
            case KEY_6: keycode = 54; break;
            case KEY_7: keycode = 55; break;
            case KEY_8: keycode = 56; break;
            case KEY_9: keycode = 57; break;
            case KEY_SEMICOLON: keycode = 59 /* ; */; break;
            case KEY_EQUAL: keycode = 61 /* = */; break;
            case KEY_A: keycode = 65; break;
            case KEY_B: keycode = 66; break;
            case KEY_C: keycode = 67; break;
            case KEY_D: keycode = 68; break;
            case KEY_E: keycode = 69; break;
            case KEY_F: keycode = 70; break;
            case KEY_G: keycode = 71; break;
            case KEY_H: keycode = 72; break;
            case KEY_I: keycode = 73; break;
            case KEY_J: keycode = 74; break;
            case KEY_K: keycode = 75; break;
            case KEY_L: keycode = 76; break;
            case KEY_M: keycode = 77; break;
            case KEY_N: keycode = 78; break;
            case KEY_O: keycode = 79; break;
            case KEY_P: keycode = 80; break;
            case KEY_Q: keycode = 81; break;
            case KEY_R: keycode = 82; break;
            case KEY_S: keycode = 83; break;
            case KEY_T: keycode = 84; break;
            case KEY_U: keycode = 85; break;
            case KEY_V: keycode = 86; break;
            case KEY_W: keycode = 87; break;
            case KEY_X: keycode = 88; break;
            case KEY_Y: keycode = 89; break;
            case KEY_Z: keycode = 90; break;
            case KEY_SPACE: keycode = 32; break;
            case KEY_APOSTROPHE: keycode = 39 /* ' */; break;
            case KEY_COMMA: keycode = 44 /* , */; break;
            case KEY_MINUS: keycode = 45 /* - */; break;
            case KEY_DOT: keycode = 46 /* . */; break;
            case KEY_SLASH: keycode = 47 /* / */; break;
            case KEY_LEFTBRACE: keycode = 91 /* [ */; break;
            case KEY_BACKSLASH: keycode = 92 /* \ */; break;
            case KEY_RIGHTBRACE: keycode = 93 /* ] */; break;
            case KEY_GRAVE: keycode = 96 /* ` */; break;
            case KEY_ESC: keycode = 256; break;
            case KEY_ENTER: keycode = 257; break;
            case KEY_TAB: keycode = 258; break;
            case KEY_BACKSPACE: keycode = 259; break;
            case KEY_INSERT: keycode = 260; break;
            case KEY_DELETE: keycode = 261; break;
            case KEY_RIGHT: keycode = 262; break;
            case KEY_LEFT: keycode = 263; break;
            case KEY_DOWN: keycode = 264; break;
            case KEY_UP: keycode = 265; break;
            case KEY_PAGEUP: keycode = 266; break;
            case KEY_PAGEDOWN: keycode = 267; break;
            case KEY_HOME: keycode = 268; break;
            case KEY_END: keycode = 269; break;
            case KEY_CAPSLOCK: keycode = 280; break;
            case KEY_SCROLLLOCK: keycode = 281; break;
            case KEY_NUMLOCK: keycode = 282; break;
            case KEY_PAUSE: keycode = 284; break;
            case KEY_F1: keycode = 290; break;
            case KEY_F2: keycode = 291; break;
            case KEY_F3: keycode = 292; break;
            case KEY_F4: keycode = 293; break;
            case KEY_F5: keycode = 294; break;
            case KEY_F6: keycode = 295; break;
            case KEY_F7: keycode = 296; break;
            case KEY_F8: keycode = 297; break;
            case KEY_F9: keycode = 298; break;
            case KEY_F10: keycode = 299; break;
            case KEY_F11: keycode = 300; break;
            case KEY_F12: keycode = 301; break;
            case KEY_F13: keycode = 302; break;
            case KEY_F14: keycode = 303; break;
            case KEY_F15: keycode = 304; break;
            case KEY_F16: keycode = 305; break;
            case KEY_F17: keycode = 306; break;
            case KEY_F18: keycode = 307; break;
            case KEY_F19: keycode = 308; break;
            case KEY_F20: keycode = 309; break;
            case KEY_F21: keycode = 310; break;
            case KEY_F22: keycode = 311; break;
            case KEY_F23: keycode = 312; break;
            case KEY_F24: keycode = 313; break;
            case KEY_KP0: keycode = 320; break;
            case KEY_KP1: keycode = 321; break;
            case KEY_KP2: keycode = 322; break;
            case KEY_KP3: keycode = 323; break;
            case KEY_KP4: keycode = 324; break;
            case KEY_KP5: keycode = 325; break;
            case KEY_KP6: keycode = 326; break;
            case KEY_KP7: keycode = 327; break;
            case KEY_KP8: keycode = 328; break;
            case KEY_KP9: keycode = 329; break;
            case KEY_KPDOT: keycode = 330; break;
            case KEY_LEFTSHIFT: keycode = 340; break;
            case KEY_LEFTCTRL: keycode = 341; break;
            case KEY_LEFTALT: keycode = 342; break;
            case KEY_RIGHTSHIFT: keycode = 344; break;
            case KEY_RIGHTCTRL: keycode = 345; break;
            case KEY_RIGHTALT: keycode = 346; break;
        }

        Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(), Nan::New(keycode));

        if (keycode == -1) {
            printf("ERROR: unknown linux key code %i\n", ev.code);
        } else {
            amino->fireEvent(event_obj);
        }
    }

    //TODO GLFW_KEY_CALLBACK_FUNCTION(ev.code, ev.value);
}

void AminoInputRPi::dumpEvent(struct input_event *event) {
    switch(event->type) {
        case EV_SYN:
            printf("EV_SYN  event separator\n");
            break;

        case EV_KEY:
            printf("EV_KEY  keyboard or button \n");

            if (event ->code == KEY_A) {
                printf("  A key\n");
            }

            if (event ->code == KEY_B) {
                printf("  B key\n");
            }

            break;

        case EV_REL:
            printf("EV_REL  relative axis\n");
            break;

        case EV_ABS:
            printf("EV_ABS  absolute axis\n");
            break;

        case EV_MSC:
            printf("EV_MSC  misc\n");

            if (event->code == MSC_SERIAL) {
                printf("  serial\n");
            }

            if (event->code == MSC_PULSELED) {
                printf("  pulse led\n");
            }

            if (event->code == MSC_GESTURE) {
                printf("  gesture\n");
            }

            if (event->code == MSC_RAW) {
                printf("  raw\n");
            }

            if (event->code == MSC_SCAN) {
                printf("  scan\n");
            }

            if (event->code == MSC_MAX) {
                printf("  max\n");
            }
            break;

        case EV_LED:
            printf("EV_LED  led\n");
            break;

        case EV_SND:
            printf("EV_SND  sound\n");
            break;

        case EV_REP:
            printf("EV_REP  autorepeating\n");
            break;

        case EV_FF:
            printf("EV_FF   force feedback send\n");
            break;

        case EV_PWR:
            printf("EV_PWR  power button\n");
            break;

        case EV_FF_STATUS:
            printf("EV_FF_STATUS force feedback receive\n");
            break;

        case EV_MAX:
            printf("EV_MAX  max value\n");
            break;

        default:
            printf("unknown\n");
            break;
    }

    printf("type = %d code = %d value = %d\n",event->type,event->code,event->value);
}