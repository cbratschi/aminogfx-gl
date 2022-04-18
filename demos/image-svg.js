const amino = require('../main.js');

//promise version (TODO change once toplevel async is supported)
(async () => {
    const gfx = new amino.AminoGfx();

    await gfx.start();

    //root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //add image view
    const screenW = gfx.w();
    const screenH = gfx.h();
    const iv = gfx.createImageView().opacity(1.0).w(screenW / 2).h(screenH);

    iv.src('demos/images/tiger.svg');
    root.add(iv);

    //resize
    gfx.on('window.size', () => {
        //log
        console.log('Window resized: ' + gfx.w() + 'x' + gfx.h());

        iv.w(gfx.w()).h(gfx.h());
    });

    //resize test
    setTimeout(() => {
        console.log('-> resizing image view');

        //set full width
        iv.w(gfx.w());
    }, 4000);
})();