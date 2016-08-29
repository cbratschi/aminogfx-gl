'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //triangle
    const model1 = gfx.createModel();

    //showTriangle(model1);
    //showTriangles(model1);
    //showRect(model1);
    showQuad(model1);

    root.add(model1);

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
});


/**
 * Simplest model.
 */
function showTriangle(model) {
    model.vertices([
        0, 0, 0,
        100, 50, 0,
        200, 200, 0 ]);
}

/**
 * Many triangles.
 */
function showTriangles(model) {
    const count = 1000;
    const vertexCount = count * 3;
    const vertices = [];
    const w = gfx.w();
    const h = gfx.h();

    for (let i  = 0; i < vertexCount; i++) {
        vertices.push(Math.random() * w);
        vertices.push(Math.random() * h);
        vertices.push((Math.random() - 0.5) * 50);
    }

    model.vertices(vertices);
}

/**
 * Rectangle.
 */
function showRect(model) {
    const x = 0;
    const y = 0;
    const w = 200;
    const h = 200;

    model.vertices([
        //triangle 1
        x, y, 0,
        x + w, y, 0,
        x, y + w, 0,

        //triangle 2
        x + w, y, 0,
        x, y + h, 0,
        x + w, y + h, 0
    ]);
}

/**
 * Quad.
 */
function showQuad(model) {
    const x = 0;
    const y = 0;
    const w = 200;
    const h = 200;

    //Note: no sorting applied
    const points = [
        [Math.random() * w, Math.random() * h],
        [Math.random() * w, Math.random() * h],
        [Math.random() * w, Math.random() * h],
        [Math.random() * w, Math.random() * h]
    ];

    // 1) using vertex coordinates only
    /*
    model.vertices([
        //triangle 1
        x + points[0][0], y + points[0][1], 0,
        x + points[1][0], y + points[1][1], 0,
        x + points[2][0], y + points[2][1], 0,

        //triangle 2
        x + points[1][0], y + points[1][1], 0,
        x + points[2][0], y + points[2][1], 0,
        x + points[3][0], y + points[3][1], 0
    ]);
    */

    // 2) using index
    model.vertices([
        //quad points
        x + points[0][0], y + points[0][1], 0,
        x + points[1][0], y + points[1][1], 0,
        x + points[2][0], y + points[2][1], 0,
        x + points[3][0], y + points[3][1], 0
    ]);

    model.indices([
        0, 1, 2, 1, 2, 3
    ]);
}

//TODO texture