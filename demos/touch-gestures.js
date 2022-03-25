'use strict';

const path = require('path');
const amino = require('../main.js');

const DEBUG = true;

(async () => {
    const gfx = new amino.AminoGfx({
        touch: {
            //Waveshare 11.9inch HDMI LCD (normal touch mode)
            invertX: true,
            invertY: true
        }
    });

    await gfx.start();

    //setup root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //show moon
    //Note: origin of model is at top/left
    const screenW = gfx.w();
    const screenH = gfx.h();
    const modelWH = Math.min(screenW, screenH) / 3;
    const model = gfx.createModel().x(screenW / 2).y(screenH / 2).w(modelWH).h(modelWH);

    createMoonModel(model);

    root.add(model);

    //setup touch gestures
    root.acceptsTouchEvents = true;

    let touch1 = null;
    let touch2 = null;
    let distanceStart;
    let lastCenterPos;

    gfx.on('touch', root, event => {
        //debug cbxx
        console.dir(event);

        //check release
        if (!event.pressed) {
            //reset
            touch1 = null;
            touch2 = null;
        }

        //check two touch points
        if (event.points !== 2) {
            return;
        }

        //init
        if (!touch1) {
            [ touch1, touch2 ] = event.points;

            distanceStart = touch1.pt.distanceTo(touch2.pt);
            lastCenterPos = touch1.pt.add(touch2.pt).div(2, 2);

            return;
        }

        //check touch points are available
        const [ tp1, tp2 ] = event.points;

        if (tp1.id !== touch1.id || tp2.id !== touch2.id) {
            return;
        }

        //handle gestures

        // 1) pinch
        const distance = tp1.pt.distanceTo(tp2.pt);
        const zoomFac = distance / distanceStart;

        if (DEBUG) {
            console.log('zoom: ' + zoomFac);
        }

        // 2) move
        const centerPos = tp1.pt.add(tp2.pt).div(2, 2);
        const diff = centerPos.distanceTo(lastCenterPos);

        lastCenterPos = centerPos;

        if (DEBUG) {
            console.log('move ' + diff.x + '/' + diff.y);
        }

        // 3) rotate
        const angle = tp1.pt.angleWith(tp2.pt);

        if (DEBUG) {
            console.log('angle: ' + angle / Math.PI * 180 + '°');
        }
        //cbxx TODO
    });
})();

/**
 * Create moon model.
 *
 * @param {*} model
 */
function createMoonModel(model) {
    //code from http://learningwebgl.com/cookbook/index.php/How_to_draw_a_sphere & http://learningwebgl.com/blog/?p=1253
    const latitudeBands = 30;
    const longitudeBands = 30;
    const radius = 100;

    const vertexPositionData = [];
    const normalData = [];
    const textureCoordData = [];

    for (let latNumber = 0; latNumber <= latitudeBands; latNumber++) {
        const theta = latNumber * Math.PI / latitudeBands;
        const sinTheta = Math.sin(theta);
        const cosTheta = Math.cos(theta);

        for (let longNumber = 0; longNumber <= longitudeBands; longNumber++) {
            const phi = longNumber * 2 * Math.PI / longitudeBands;
            const sinPhi = Math.sin(phi);
            const cosPhi = Math.cos(phi);
            const x = cosPhi * sinTheta;
            const y = cosTheta;
            const z = sinPhi * sinTheta;
            const u = 1 - (longNumber / longitudeBands);
            const v = latNumber / latitudeBands;

            normalData.push(x);
            normalData.push(-y); //y-inversion
            normalData.push(z);

            textureCoordData.push(u);
            textureCoordData.push(1. - v); //y-inversion

            vertexPositionData.push(radius * x);
            vertexPositionData.push(- radius * y); //y-inversion
            vertexPositionData.push(radius * z);
        }
    }

    const indexData = [];

    for (let latNumber = 0; latNumber < latitudeBands; latNumber++) {
        for (let longNumber = 0; longNumber < longitudeBands; longNumber++) {
            const first = (latNumber * (longitudeBands + 1)) + longNumber;
            const second = first + longitudeBands + 1;

            indexData.push(first);
            indexData.push(second);
            indexData.push(first + 1);

            indexData.push(second);
            indexData.push(second + 1);
            indexData.push(first + 1);
        }
    }

    //setup model
    model.vertices(vertexPositionData);
    model.indices(indexData);
    model.normals(normalData);
    model.uvs(textureCoordData);

    //texture (http://planetpixelemporium.com/earth.html)
    model.src(path.join(__dirname, 'tests/sphere/moonmap1k.jpg'));
    //model.src(path.join(__dirname, 'sphere/earthmap1k.jpg'));

    //animate
    model.rx.anim().from(0).to(360).dur(5000).loop(-1).start();
    model.ry.anim().from(0).to(360).dur(5000).loop(-1).start();
}