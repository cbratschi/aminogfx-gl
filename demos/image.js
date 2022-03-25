const amino = require('../main.js');

//promise version (TODO change once toplevel async is supported)
(async () => {
    const gfx = new amino.AminoGfx();

    await gfx.start();

    //root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //add image view
    const iv = gfx.createImageView().opacity(1.0).w(200).h(200);

    iv.src('demos/slideshow/images/iTermScreenSnapz001.png');
    root.add(iv);

    //add rect
    const rect = gfx.createRect().w(20).h(30).fill('#ff00ff');

    root.add(rect);
})();