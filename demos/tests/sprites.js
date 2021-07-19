'use strict';

const path = require('path');
const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

//see https://freepngimg.com/png/94199-material-sprite-opengameartorg-photography-asteroids-monochrome
const spriteXY = 8;
const spriteImgWH = 1024;
const spriteImgItemWH = spriteImgWH / spriteXY;
const spriteWH = 1 / spriteXY;

let spriteX = 0;
let spriteY = 0;

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = this.createGroup();

    this.setRoot(root);

    //show info
    console.log('Sprite: ' + spriteImgItemWH + 'x' + spriteImgItemWH);

    //add image view
    const iv = this.createImageView().size('stretch').w(spriteImgItemWH).h(spriteImgItemWH);

    iv.src(path.join(__dirname, 'sprites/94199-material-sprite-opengameartorg-photography-asteroids-monochrome.png'));
    root.add(iv);

    //sprite animation
    setSpritePos(iv);

    iv.image.watch(() => {
        //show info
        console.log('Sprite loaded.');

        setInterval(() => {
            nextSpriteImage();
            setSpritePos(iv);
        }, 100);
    });
});

/**
 * Set the sprite position.
 *
 * @param {ImageView} iv
 */
function setSpritePos(iv) {
    const left = spriteX * spriteWH;
    const right = left + spriteWH;
    const top = spriteY * spriteWH;
    const bottom = top + spriteWH;

    iv.attr({
        left,
        right,
        top,
        bottom
    });
}

/**
 * Set the next sprite image.
 */
function nextSpriteImage() {
    spriteX++;

    if (spriteX >= spriteXY) {
        spriteX = 0;
        spriteY = (spriteY + 1) % spriteXY;
    }

    /*
    if (spriteY === 4) {
        spriteY = 0;
    }
    */

    //debug
    //console.log(spriteX + ' ' + spriteY);
}