var amino = require('../main.js');

amino.start(function(core, stage) {
    //stage.setRoot(rect);
    stage.fill("#00ff00");
//    stage.opacity(0.0);
    var root = new amino.Group();
    stage.setRoot(root);
    var rect = new amino.Rect().w(200).h(250).x(0).y(0).fill("#0000ff");
    root.add(rect);

    var circle = new amino.Circle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(100).y(100);
    //root.x(100);

    core.on('click',circle,function() {
        console.log("clicked on the circle");
    });
    core.on("keypress",null,function() {
        console.log("key was pressed");
    });

    root.add(circle);
});
