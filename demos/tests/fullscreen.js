'use strict';

const amino = require('../../main.js');

// 1) create amino instance
const gfx = new amino.AminoGfx({
    fullscreen: true
});

//check monitor
const stats = gfx.getStats();
const monitor = gfx.getMonitor();
const monitors = gfx.getMonitors();

console.log('Stats:');
console.dir(stats);

console.log();
console.log('Monitor:');
console.dir(monitor);

console.log();
console.log('Monitors:');
console.dir(monitors);

//some info
console.log();
console.log('Screen:');
console.dir(gfx.screen);

// 2) select external display if available
if (monitors.length > 1) {
    //report
    console.log('-> using external monitor');

    //use second monitor
    const monitor = monitors[1];

    // 3) use 1080p@60 if available
    /*
        width: 1920,
        height: 1080,
        redBits: 8,
        blueBits: 8,
        greenBits: 8,
        refreshRate: 60
    */
    const mode = monitor.modes.find(mode => mode.width === 1920 && mode.height === 1080 && mode.refreshRate === 60);

    if (mode) {
        //report
        console.log('-> using 1080p@60 display mode');
    }

    gfx.setMonitor({
        monitor,
        mode
    });
} else {
    //find other resolution
    //cbxx TODO verify
    const currentMode = monitor.mode;
    const mode = monitor.modes.find(mode => mode.width !== currentMode.width || mode.height !== currentMode.height || mode.refreshRate !== currentMode.refreshRate);

    if (mode) {
        //report
        console.log('-> switching to different mode');

        //Note: only switch mode (keeps current monitor)
        gfx.setMonitor({
            mode
        });
    }
}

// 4) start render process
gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //info
    //console.log('runtime: ' + JSON.stringify(gfx.runtime));
    console.dir(gfx.getMonitor());
    console.dir(gfx.screen);
});