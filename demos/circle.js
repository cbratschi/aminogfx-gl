'use strict';

const path = require('path');
const amino = require('../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //green background
    this.fill('#00ff00');

    //setup root
    const root = this.createGroup();

    this.setRoot(root);

    //blue rectangle (at 0/0)
    const rect = this.createRect().w(200).h(250).x(0).y(0).fill('#0000ff');

    root.add(rect);
    rect.acceptsMouseEvents = true;
    rect.acceptsKeyboardEvents = true;

    //cbxx TODO auto convert touch to mouse events
    this.on('key.press', rect, event => {
        console.log('key was pressed', event.keycode, event.printable, event.char);
    });

    this.on('drag', rect, event => {
        //move rectangle
        const delta = event.delta;

        rect.x(rect.x() + delta.x);
        rect.y(rect.y() + delta.y);
    });

    this.on('press', rect, _event => {
        //set red
        rect.fill('#ff0000');
    });

    this.on('release', rect, _event => {
        //set blue
        rect.fill('#0000ff');
    });

    this.on('click', rect, function () {
        console.log('clicked on the rect');
    });

    //circle (at 100/100)
    const circle = this.createCircle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(100).y(100);

    circle.acceptsMouseEvents = true;

    this.on('click', circle, function () {
        console.log('clicked on the circle');
    });

    root.add(circle);

    //images

    //tree at 0/0
    const iv = this.createImageView();

    iv.src(path.join(__dirname, '/images/tree.png'));
    iv.sx(4).sy(4);
    root.add(iv);

    //yosemite animated
    const iv2 = this.createImageView();

    iv2.src(path.join(__dirname, '/images/yose.jpg'));
    iv2.sx(4).sy(4);
    iv2.x(300);
    root.add(iv2);

    //jumps from 300 to 0 before animation starts
    iv2.x.anim().delay(1000).from(0).to(1000).dur(3000).start();
});
