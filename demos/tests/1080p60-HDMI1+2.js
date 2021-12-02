'use strict';
// attempts to create two instances of amino, one on each screen
// requires a Pi 4 with two HDMI displays connected

const amino = require('../../main.js');
function make1() { // 1
    //create instance
    const gfx = new amino.AminoGfx({
        resolution: '1080p@60',

        //multi-display support
        display: 'HDMI-A-1' //Pi 4: HDMI 2
    });

    gfx.start(function (err) {
        if (err) {
            console.log('Amino error: ' + err.message);
            return;
        }

        this.fill('#FF0000');

        //some info
        console.log('#1 screen: ' + JSON.stringify(gfx.screen));
        console.log('#1 window size: ' + this.w() + 'x' + this.h());
        console.log('#1 runtime: ' + JSON.stringify(gfx.runtime));

        console.log("screen #1 intitialized, starting 2...");
        make2();
    });
}
function make2() { // 2
    //create instance
    const gfx = new amino.AminoGfx({
        resolution: '1080p@60',

        //multi-display support
        display: 'HDMI-A-2' //Pi 4: HDMI 2
    });

    gfx.start(function (err) {
        if (err) {
            console.log('Amino error: ' + err.message);
            return;
        }

        this.fill('#00FF00');

        //some info
        console.log('#2 screen: ' + JSON.stringify(gfx.screen));
        console.log('#2 window size: ' + this.w() + 'x' + this.h());
        console.log('#2 runtime: ' + JSON.stringify(gfx.runtime));
    });
}

make1();