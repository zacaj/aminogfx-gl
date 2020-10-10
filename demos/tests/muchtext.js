'use strict';

// text stress test

/*eslint no-unused-vars: 0*/

const amino = require('../../main.js');
const path = require('path');

//func call test (must fail)

function a() {
    //basic tests
    const gfx = new amino.AminoGfx({resolution: '1080p@60',display: 'HDMI-A-1'});

    console.log('instance: ' + gfx);

    //screen
    console.log('screen: ' + JSON.stringify(gfx.screen));

    //default size
    console.log('default size: ' + gfx.w() + 'x' + gfx.h());

    //start
    gfx.start(start(gfx));
}
var B = false;
function b() {
    console.log('b');
    if (B) return;
    B= true;
    console.log('start b');
    //basic tests
    const gfx = new amino.AminoGfx({resolution: '1080p@60',display: 'HDMI-A-2'});

    console.log('instance: ' + gfx);

    //screen
    console.log('screen: ' + JSON.stringify(gfx.screen));

    //default size
    console.log('default size: ' + gfx.w() + 'x' + gfx.h());

    //start
    gfx.start(start(gfx));
}
a();

function start(gfx) { return function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    console.log('started');

    //runtime
    console.log('runtime: ' + JSON.stringify(gfx.runtime));

    //show position
    console.log('position: ' + this.x() + '/' + this.y());
    //this.x(0).y(0);

    //modify size
    this.w(400);
    this.h(400);
    this.fill('#0000FF');

    //create group
    const g = this.createGroup();

    g.w.bindTo(this.w);
    g.h.bindTo(this.h);

    this.setRoot(g);

    //add rect
    const r = this.createRect().w(100).h(100).fill('#FF0000');

    r.originX(.5).originY(.5).rz(45);

    g.add(r);


    //text

    b();

    const tg = this.createGroup();
    g.add(tg);

    setInterval(() => {
        if (tg.children.length > 5)
            tg.clear();
        const text = this.createText()
            .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
            .fontSize(80)
            .x(tg.children.length).y(0)
            .vAlign('top')
            .w(300)
            .h(160)
            .wrap('word')
            .fill('#ffff00');
        tg.add(text);

    }, 1);
};}