'use strict';

const amino = require('../main.js');

(async () => {
    const gfx = new amino.AminoGfx({
        touch: {
            //Waveshare 11.9inch HDMI LCD (normal touch mode)
            invertX: true,
            invertY: true
        }
    });

    await gfx.start();

    //test horz
    //gfx.w(320);
    //gfx.h(240);

    //test vert
    //gfx.w(240);
    //gfx.h(320);

    //setup root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //add three rectangles
    const screenW = gfx.w();
    const screenH = gfx.h();
    const buttonWH = Math.min(screenW, screenH) / 3;
    const buttonCount = 3;

    if (screenW > screenH) {
        const space = (screenW - buttonCount * buttonWH) / (buttonCount + 1);
        let xPos = space;
        let yPos = buttonWH;

        for (let i = 0; i < buttonCount; i++) {
            createButton(xPos, yPos);
            xPos += buttonWH + space;
        }
    } else {
        const space = (screenH - buttonCount * buttonWH) / (buttonCount + 1);
        let yPos = space;
        let xPos = buttonWH;

        for (let i = 0; i < buttonCount; i++) {
            createButton(xPos, yPos);
            yPos += buttonWH + space;
        }
    }

    function createButton(x, y) {
        const color = '#0000ff';
        const colorPressed = '#ff0000';
        const rect = gfx.createRect().w(buttonWH).h(buttonWH).x(x).y(y).fill(color);

        //debug
        //console.log('Button: ' + x + ' ' + y);

        //mouse and touch events (supports multiple touch points)
        rect.acceptsMouseEvents = true;

        let buttonPressed = -1;

        gfx.on('press', rect, event => {
            if (buttonPressed !== -1) {
                return;
            }

            rect.fill(colorPressed);

            buttonPressed = event.button;
        });

        gfx.on('drag', rect, event => {
            //move rectangle
            const delta = event.delta;

            rect.x(rect.x() + delta.x);
            rect.y(rect.y() + delta.y);
        });


        gfx.on('release', rect, event => {
            if (buttonPressed !== event.button) {
                return;
            }

            buttonPressed = -1;

            rect.fill(color);
        });


        //add
        root.add(rect);
    }
})();