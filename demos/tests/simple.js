'use strict';

const amino = require('../../main.js');

//func call test (must fail)
//console.log('res: ' + amino.AminoGfx());

//basic tests
const gfx = new amino.AminoGfx();

console.log('instance: ' + gfx);

//screen
console.log('screen: ' + JSON.stringify(gfx.screen));

//default size
console.log('default size: ' + gfx.w() + 'x' + gfx.h());

//start
gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    console.log('started');

    //runtime
    console.log('runtime: ' + JSON.stringify(gfx.runtime));

    //modify size
    this.w(200);
    this.h(200);
    this.fill('#0000FF');

    //TODO more cbx
});