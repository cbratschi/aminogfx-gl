'use strict';

/*
 * the job of this class is to route input events and calculate semantic events.
 * it does not map from platform specific events to generic ones. that is the job
 * of the inputevents module
 */

const DEBUG = true; //cbxx

const IE = require('./inputevents');

/**
 * Create a point.
 */
function makePoint(x, y) {
    return {
        x,
        y,

        /**
         * Subtract a point or negate coordinates.
         *
         * @returns
         */
        minus: function () {
            if (arguments.length === 1) {
                const pt = arguments[0];

                return makePoint(this.x - pt.x, this.y - pt.y);
            }

            if (arguments.length === 2) {
                const xy = arguments;

                return makePoint(this.x - xy[0], this.y - xy[1]);
            }
        },

        /**
         * Divide by x and y value.
         *
         * @param {*} x
         * @param {*} y
         * @returns
         */
        divide: function (x, y) {
            return makePoint(this.x / x, this.y / y);
        },

        /**
         * Multiply by x and y factor.
         *
         * @param {*} x
         * @param {*} y
         * @returns
         */
        multiply: function (x, y) {
            return makePoint(this.x * x, this.y * y);
        },

        /**
         * Calc the distance to a point.
         *
         * @param {*} pt
         * @returns
         */
        distanceTo(pt) {
            const xDist = this.x - pt.x;
            const yDist = this.y - pt.y;

            return Math.sqrt(xDist * xDist + yDist * yDist);
        },

        /**
         * Calc angle spanned by points.
         *
         * @param {*} pt
         * @returns
         */
        angleWith(pt) {
            const a = pt.y - this.y;
            const b = pt.x - this.x;
            let alpha = Math.atan(a / Math.abs(b));

            //cbxx TODO verify angle
            if (b < 0) {
                //map quadrant
                if (alpha > 0) {
                    alpha = Math.PI - alpha;
                } else {
                    alpha = - Math.PI + alpha;
                }
            }

            //normalize results
            if (Number.isNaN(alpha)) {
                return 0;
            }

            return alpha;
        }
    };
}

exports.makePoint = makePoint;

/**
 * Initialize this module.
 */
exports.init = function () {
    if (DEBUG) {
        console.log('initializing input');
    }

    IE.init();
};

//
// AminoEvents
//

function AminoEvents(gfx) {
    this.gfx = gfx;

    //properties
    this.listeners = {};
    this.focusObjects = {
        pointer: {
            target: null
        },
        scroll: {
            target: null
        },
        keyboard: {
            target: null
        }
    };

    this.statusObjects = {
        pointer: {
            pt: makePoint(-1, -1),
            prevPt: makePoint(-1, -1)
        },
        keyboard: {
            state: {}
        }
    };

    //touch
    this.touchMap = {};
    this.touchPoints = [];
}

/**
 * Create new instance.
 */
exports.createEventHandler = function (gfx) {
    if (DEBUG) {
        console.log('createEventHandler()');
    }

    return new AminoEvents(gfx);
};

const handlers = {
    //mouse
    'mouse.position': function (obj, evt) {
        const s = obj.statusObjects.pointer;

        s.prevPt = s.pt;
        s.pt = makePoint(evt.x, evt.y);

        const target = obj.focusObjects.pointer.target;

        if (target != null && obj.statusObjects.pointer.state === 1) {
            obj.sendDragEvent(evt);
        }
    },
    'mouse.button': function (obj, evt) {
        if (evt.button === 0) {
            //left mouse button
            if (DEBUG) {
                console.log('left mousebutton event');
            }

            obj.statusObjects.pointer.state = evt.state;

            //pressed
            if (evt.state === 1) {
                const pts = obj.statusObjects.pointer;

                if (DEBUG) {
                    console.log('-> pressed');
                }

                //cbxx TODO focus
                obj.setupPointerFocus(pts.pt);
                obj.sendPressEvent(evt);
                return;
            }

            //released
            if (evt.state === 0) {
                if (DEBUG) {
                    console.log('-> released');
                }

                obj.sendReleaseEvent(evt);
                obj.stopPointerFocus();
                return;
            }
        }
    },
    'mousewheel.v': function (obj, evt) {
        const pts = obj.statusObjects.pointer;

        obj.setupScrollFocus(pts.pt);
        obj.sendScrollEvent(evt);
    },
    //touch
    touch: (obj, evt) => {
        //debug
        //console.log('Handle touch event:');
        //console.dir(evt);

        obj.handleTouchEvent(evt);
    },
    //keyboard
    'key.press': function (obj, evt) {
        obj.statusObjects.keyboard.state[evt.keycode] = true;

        const evt2 = IE.fromAminoKeyboardEvent(evt, obj.statusObjects.keyboard.state);

        obj.sendKeyboardPressEvent(evt2);
    },
    'key.release': function (obj, evt) {
        obj.statusObjects.keyboard.state[evt.keycode] = false;
        obj.sendKeyboardReleaseEvent(IE.fromAminoKeyboardEvent(evt, obj.statusObjects.keyboard.state));
    },
    //window
    'window.size': function (obj, evt) {
        obj.fireEventAtTarget(null, evt);
    },
    'window.close': function (obj, evt) {
        obj.fireEventAtTarget(null, evt);
    }
};

/**
 * Process an event.
 */
AminoEvents.prototype.processEvent = function (evt) {
    if (DEBUG) {
        console.log('processEvent() ' + JSON.stringify(evt));
    }

    const handler = handlers[evt.type];

    if (handler) {
        return handler(this, evt);
    }

    //console.log('unhandled event', evt);
};

/**
 * Register event handler.
 */
AminoEvents.prototype.on = function (name, target, listener) {
    //name
    if (!name) {
        throw new Error('missing name');
    }

    name = name.toLowerCase();

    if (!this.listeners[name]) {
        this.listeners[name] = [];
    }

    //special case (e.g. window.size handler)
    if (!listener) {
        //two parameters set
        listener = target;
        target = null;
    }

    //add
    this.listeners[name].push({
        target,
        func: listener
    });
};

/**
 * Set pointer focus on node.
 */
AminoEvents.prototype.setupPointerFocus = function (pt) {
    if (DEBUG) {
        console.log('setupPointerFocus()');
    }

    //get all nodes
    const nodes = this.gfx.findNodesAtXY(pt, node => {
        //filter opt-out
        if (node.children && node.acceptsMouseEvents === false) {
            return false;
        }

        return true;
    });

    //get nodes accepting mouse events
    const pressNodes = nodes.filter(node => {
        return node.acceptsMouseEvents === true;
    });

    if (pressNodes.length > 0) {
        this.focusObjects.pointer.target = pressNodes[0];
    } else {
        this.focusObjects.pointer.target = null;
    }

    const keyboardNodes = nodes.filter(function (n) {
        return n.acceptsKeyboardEvents === true;
    });

    if (keyboardNodes.length > 0) {
        if (this.focusObjects.keyboard.target) {
            this.fireEventAtTarget(this.focusObjects.keyboard.target, {
                type: 'focus.lose',
                target: this.focusObjects.keyboard.target,
            });
        }

        this.focusObjects.keyboard.target = keyboardNodes[0];

        this.fireEventAtTarget(this.focusObjects.keyboard.target, {
            type: 'focus.gain',
            target: this.focusObjects.keyboard.target,
        });
    } else {
        if (this.focusObjects.keyboard.target) {
            this.fireEventAtTarget(this.focusObjects.keyboard.target, {
                type: 'focus.lose',
                target: this.focusObjects.keyboard.target,
            });
        }

        this.focusObjects.keyboard.target = null;
    }
};

AminoEvents.prototype.stopPointerFocus = function () {
    this.focusObjects.pointer.target = null;
};

AminoEvents.prototype.sendPressEvent = function (e) {
    const node = this.focusObjects.pointer.target;

    if (!node) {
        return;
    }

    const pt = this.gfx.globalToLocal(this.statusObjects.pointer.pt, node);

    this.fireEventAtTarget(node, {
        type: 'press',
        button: e.button,
        point: pt,
        target: node
    });
};

AminoEvents.prototype.sendReleaseEvent = function (e) {
    const node = this.focusObjects.pointer.target;

    if (!node) {
        return;
    }

    //release
    const pt = this.gfx.globalToLocal(this.statusObjects.pointer.pt, node);

    this.fireEventAtTarget(node, {
        type: 'release',
        button: e.button,
        point: pt,
        target: node,
    });

    //click
    if (node.contains(pt)) {
        this.fireEventAtTarget(node, {
            type: 'click',
            button: e.button,
            point: pt,
            target: node,
        });
    }
};

AminoEvents.prototype.sendDragEvent = function (e) {
    const node = this.focusObjects.pointer.target;
    const s = this.statusObjects.pointer;

    if (!node) {
        return;
    }

    const localPt = this.gfx.globalToLocal(s.pt, node);
    const localPrev = this.gfx.globalToLocal(s.prevPt, node);

    this.fireEventAtTarget(node, {
        type: 'drag',
        button: e.button,
        point: localPt,
        delta: localPt.minus(localPrev),
        target: node
    });
};

AminoEvents.prototype.setupScrollFocus = function (pt) {
    let nodes = this.gfx.findNodesAtXY(pt);

    nodes = nodes.filter(function (n) {
        return n.acceptsScrollEvents === true;
    });

    if (nodes.length > 0) {
        this.focusObjects.scroll.target = nodes[0];
    } else {
        this.focusObjects.scroll.target = null;
    }
};

AminoEvents.prototype.sendScrollEvent = function (e) {
    const target = this.focusObjects.scroll.target;

    if (!target) {
        return;
    }

    this.fireEventAtTarget(target, {
        type: 'scroll',
        target,
        position: e.position,
    });
};

AminoEvents.prototype.sendKeyboardPressEvent = function (event) {
    if (this.focusObjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.press';
    event.target = this.focusObjects.keyboard.target;

    this.fireEventAtTarget(event.target, event);
};

AminoEvents.prototype.sendKeyboardReleaseEvent = function (event) {
    if (this.focusObjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.release';
    event.target = this.focusObjects.keyboard.target;

    this.fireEventAtTarget(event.target, event);
};

/**
 * Fire an event.
 *
 * @param {*} target
 * @param {*} event
 */
AminoEvents.prototype.fireEventAtTarget = function (target, event) {
    if (DEBUG) {
        console.log('firing an event at target:', event.type, target ? target.id():'');
    }

    if (!event.type) {
        console.log('Warning: event has no type!');
    }

    const funcs = this.listeners[event.type];

    if (funcs) {
        for (const item of funcs) {
            if (item.target === target) {
                item.func(event);
            }
        }
    }
};

/**
 * Handle touch events.
 *
 * @param {*} evt
 */
AminoEvents.prototype.handleTouchEvent = function (evt) {
    const lastTouchMap = this.touchMap;
    const lastTouchPoints = this.touchPoints;

    //check end of touch (all)
    if (!evt.pressed) {
        //fire release
        for (const item of lastTouchPoints) {
            const target = item.target;

            if (target) {
                //fire release/click
                const pt = makePoint(item.x, item.y);

                this.fireTouchRelease(item, pt, target);
            }
        }

        //reset
        this.touchMap = {};
        this.lastTouchPoints = [];

        //check touch
        if (this.touchNode) {
            //fire touch done
            this.fireTouchEvent(evt, this.touchNode, 'done');
            this.touchNode = null;
        }

        return;
    }

    //check touch
    if (this.touchNode) {
        //fire touch update
        this.fireTouchEvent(evt, this.touchNode, 'update');
        return;
    }

    //check touch points
    const touchMap = {};
    const touchPoints = [];
    let pos = 0;

    for (const item of evt.points) {
        const id = item.id;
        const prevItem = lastTouchMap[id];

        //store
        touchMap[id] = item;
        touchPoints.push(item);

        if (prevItem) {
            //touch point exists
            const target = prevItem.target;

            if (target) {
                item.target = target;

                //send drag event
                const pt = makePoint(item.x, item.y);
                const localPt = this.gfx.globalToLocal(pt, target);
                const localPrev = this.gfx.globalToLocal(prevItem.pt, target);

                item.pt = pt;

                this.fireEventAtTarget(target, {
                    type: 'drag',
                    touch: true,
                    button: item.id,
                    point: localPt,
                    delta: localPt.minus(localPrev),
                    target
                });
            }
        } else {
            //new touch point
            const pt = makePoint(item.x, item.y);

            item.pt = pt;

            //find node
            const target = this.getTouchNodeAtXY(pt);

            item.target = target;

            if (!target) {
                continue;
            }

            //check touch
            if (pos === 0 && target.acceptsTouchEvents) {
                this.touchNode = target;

                if (DEBUG) {
                    console.log('-> touch start');
                }

                this.fireTouchEvent(evt, target, 'start');

                break;
            }

            this.touchNode = null;

            //emulate mouse events (button is the id of the touch point)

            //debug
            //console.log('Found target:');
            //console.dir(target);

            //fire press
            const localPt = this.gfx.globalToLocal(pt, target);

            this.fireEventAtTarget(target, {
                type: 'press',
                touch: true,
                button: item.id,
                point: localPt,
                target
            });
        }

        pos++;
    }

    //check prev touch points
    for (const item of lastTouchPoints) {
        //check released
        if (!touchMap[item.id]) {
            const target = item.target;

            if (target) {
                //fire release/click
                const pt = makePoint(item.x, item.y);

                this.fireTouchRelease(item, pt, target);
            }
        }
    }

    this.touchMap = touchMap;
    this.touchPoints = touchPoints;
};

/**
 * Get a touch capabable node.
 *
 * @param {*} pt
 * @returns
 */
AminoEvents.prototype.getTouchNodeAtXY = function (pt) {
    const nodes = this.gfx.findNodesAtXY(pt, node => {
        //filter opt-out
        if (node.children && node.acceptsMouseEvents === false && node.acceptsTouchEvents === false) {
            return false;
        }

        return true;
    }).filter(node => {
        return node.acceptsMouseEvents === true || node.acceptsTouchEvents === true;
    });

    if (nodes.length > 0) {
        //node found
        return nodes[0];
    }

    return null;
};

/**
 * Fire a touch event.
 *
 * @param {*} event
 * @param {*} target
 * @param {*} action
 */
AminoEvents.prototype.fireTouchEvent = function (event, target, action) {
    //map points
    const points = [];

    for (const item of event.points) {
        points.push({
            id: item.id,
            count: item.count,
            pt: this.gfx.globalToLocal(makePoint(item.x, item.y), target),
            time: item.time
        });
    }

    //fire touch start
    this.fireEventAtTarget(target, {
        type: 'touch',
        action,
        points,
        pressed: event.pressed,
        target
    });
};

/**
 * Fire release or click events.
 *
 * @param {*} item
 * @param {*} pt
 * @param {*} target
 */
AminoEvents.prototype.fireTouchRelease = function (item, pt, target) {
    const localPt = this.gfx.globalToLocal(pt, target);

    this.fireEventAtTarget(target, {
        type: 'release',
        touch: true,
        button: item.id,
        point: localPt,
        target
    });

    if (DEBUG) {
        console.log('-> touch release');
    }

    //click
    if (target.contains(localPt)) {
        this.fireEventAtTarget(target, {
            type: 'click',
            touch: true,
            button: item.id,
            point: localPt,
            target
        });

        if (DEBUG) {
            console.log('-> touch click');
        }
    }
};