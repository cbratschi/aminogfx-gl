'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play video file.
 */

if (process.argv.length !== 3) {
    console.log('missing video file parameter');
    return;
}

const file = process.argv[2];

player.playVideo({
    src: path.join(process.cwd(), file)
}, (err, video) => {
    //empty
});